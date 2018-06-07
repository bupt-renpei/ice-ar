#define ENABLE_LOG

using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using Vectrosity;
using GoogleARCore;
using GoogleARCoreInternal;
using TextureReaderAdapted;
using System.Threading;
using DisruptorUnity3d;
using PlaynomicsPlugin;
using Kalman;
using System;
using UnityEngine.Rendering;

public class OnCameraFrame : MonoBehaviour, ILogComponent
{

    public UnityEngine.UI.Text textbox;
    public double timestamp_;
    public System.DateTime begin_;
    public FramePoolManager frameMgr_;
    public BoundingBoxPoolManager boxMgr_;
    public ImageController imageController_;

    private FaceProcessor faceProcessor_;
    private AnnotationsFetcher yoloFetcher_;
    private AnnotationsFetcher openFaceFetcher_;
    private AssetBundleFetcher assetFetcher_;
    private SemanticDbController dbController_;
    private float dbQueryRate_;
    private DateTime lastDbQuery_;
    private DateTime lastKeyFrame_; //Used to keep the updating of UI elements roughly in sync with DB query rate

    public bool renderBoundingBoxes;


    private ConcurrentQueue<Dictionary<int, FrameObjectData>> frameBuffer_;
    private static ConcurrentQueue<BoxData> boundingBoxBufferToCalc_;
    private static ConcurrentQueue<CreateBoxData> boundingBoxBufferToUpdate_;
    public List<CreateBoxData> boxData_;
    //public RingBuffer<FrameObjectData> frameObjectBuffer;
    //public static RingBuffer<BoxData> boxBufferToCalc;
    //public static RingBuffer<CreateBoxData> boxBufferToUpdate;
    public List<Color> colors_;
    Camera camForCalcThread_;
    Thread calc_;
    public Dictionary<string, Color> labelColors_;
    public Dictionary<string, IKalmanWrapper> kalman_;
    public TextureReader TextureReaderComponent;
    public ARCoreBackgroundRenderer BackgroundRenderer;

    void Awake()
    {
        QualitySettings.vSyncCount = 0;  // VSync must be disabled
        Application.targetFrameRate = 30;
        begin_ = System.DateTime.Now;
    }

    public void changeDbQueryRate(float newRate)
    {
        dbQueryRate_ = 1 / newRate;
    }

    public void changeBoundingBoxRendering(bool render)
    {
        renderBoundingBoxes = render;
    }

    void Start()
    {
        Debug.Message("initializing OnCameraFrame...");

        try
        {
            renderBoundingBoxes = true;
            timestamp_ = 0;

            Debug.Log("adding callback for capturing camera frames");
            TextureReaderComponent.OnImageAvailableCallback += OnImageAvailable;

            frameMgr_ = GameObject.FindObjectOfType<FramePoolManager>();
            boxMgr_ = GameObject.FindObjectOfType<BoundingBoxPoolManager>();

            Debug.Log("creating structures for frames and bounding boxes processing");
            frameBuffer_ = new ConcurrentQueue<Dictionary<int, FrameObjectData>>();
            boundingBoxBufferToCalc_ = new ConcurrentQueue<BoxData>();
            boundingBoxBufferToUpdate_ = new ConcurrentQueue<CreateBoxData>();
            boxData_ = new List<CreateBoxData>();

            camForCalcThread_ = GameObject.Find("Camera").GetComponent("Camera") as Camera;
            //calc_ = new Thread(calculationsForBoundingBox);
            //calc_.Start();

            labelColors_ = new Dictionary<string, Color>();
            kalman_ = new Dictionary<string, IKalmanWrapper>();

            colors_ = new List<Color> {
                new Color (255f/255, 109f/255, 124f/255),
                new Color (119f/255, 231f/255, 255f/255),
                new Color (82f/255, 255f/255, 127f/255),
                new Color (252f/255, 187f/255, 255f/255),
                new Color (255f/255, 193f/255, 130f/255)
            };

            Debug.Log("initializing semantic db controller");
            lastDbQuery_ = System.DateTime.Now;
            lastKeyFrame_ = System.DateTime.Now;
            dbQueryRate_ = 0.5f; // once every 2 seconds
            dbController_ = new SemanticDbController("http://131.179.142.7:8888/query");

            Debug.Log("initializing NDN modules");
            // @Therese - these need to be moved somewhere to a higher-level entity as
            // configuration parameters (may be changed frequently during testing)
            string rootPrefix = "/icear/user";
            string userId = "peter"; // "mobile-terminal0";
            string serviceType = "object_recognizer";
            string serviceInstance = "yolo"; // "yolo";
            string serviceInstance2 = "openface"; // "yolo";

            NdnRtc.Initialize(rootPrefix, userId);
            faceProcessor_ = new FaceProcessor();
            faceProcessor_.start();

            assetFetcher_ = new AssetBundleFetcher(faceProcessor_);

            string servicePrefix = rootPrefix + "/" + userId + "/" + serviceType;
            // AnnotationsFetcher instance might also be a singleton class
            // and initialized/created somewhere else. here just as an example
            yoloFetcher_ = new AnnotationsFetcher(faceProcessor_, servicePrefix, serviceInstance);
            openFaceFetcher_ = new AnnotationsFetcher(faceProcessor_, servicePrefix, serviceInstance2);

            // setup CNL logging 
            //ILOG.J2CsMapping.Util.Logging.Logger.getLogger("").setLevel(ILOG.J2CsMapping.Util.Logging.Level.FINE);
            //ILOG.J2CsMapping.Util.Logging.Logger.Write = delegate (string message) { Debug.Log(System.DateTime.Now + ": " + message); };
        }
        catch (System.Exception e)
        {
            Debug.LogExceptionFormat(e, "while initializing");
        }
    }

    public void OnDestroy()
    {

    }


    void Update()
    {
        calculationsForBoundingBox();

        Debug.LogFormat("running update for {0} bounding boxes", boundingBoxBufferToUpdate_.Count);
        try
        {
            for (int i = 0; i < boundingBoxBufferToUpdate_.Count; i++)
            {
                CreateBoxData temp = boundingBoxBufferToUpdate_.Dequeue();
                Debug.Log("frame number for box: " + temp.frameNum);
                textbox.text = "Yolo " + temp.frameNum;
                Debug.Log("queue size: " + i);
                Color c = colors_[UnityEngine.Random.Range(0, colors_.Count)];
                List<BoundingBox> boundingBoxes;
                CreateBoxData box = new CreateBoxData();
                bool updatedBox = false;
                //found color for this label
                //boxMgr.CreateBoundingBoxObject (temp.position, temp.x, temp.y, temp.z, temp.label, c);

                if (boxMgr_.boundingBoxObjects.TryGetValue(temp.label, out boundingBoxes))
                {
                    //Debug.Log ("Update found label");
                    for (int j = 0; j < boundingBoxes.Count; j++)
                    {
                        //find bounding box and label that matches
                        //Debug.Log ("Update searching list for box");
                        //float distance = Vector3.Distance (temp.position, boundingBoxes [j].box.transform.position);
                        float distance = Vector2.Distance(Camera.main.WorldToViewportPoint(temp.position), Camera.main.WorldToViewportPoint(boundingBoxes[j].box.transform.position));
                        //Vector3 direction = new Vector3 (temp.position.x - boundingBoxes [j].box.transform.position.x, temp.position.y - boundingBoxes [j].box.transform.position.y, temp.position.z - boundingBoxes [j].box.transform.position.z);

                        //float speed = distance / (Mathf.Abs ((float)(temp.timestamp - offset.m_screenUpdateTime)));
                        //Debug.Log("Distance and speed: " + distance + ", " + speed);
                        if (distance < 0.2f)
                        {
                            //Debug.Log ("Update found box");
                            Vector3 filteredPos = kalman_[boundingBoxes[j].guid].Update(temp.position);
                            boxMgr_.UpdateBoundingBoxObject(boundingBoxes[j], temp.position, temp.x, temp.y, temp.z, temp.label, temp.position);
                            Debug.Log("Update bounding box: " + temp.label);
                            updatedBox = true;
                        }
                    }
                    //none of the labels looked like the wanted box, must be a new instance of this label
                    if (!updatedBox)
                    {
                        box.position = temp.position;
                        box.x = temp.x;
                        box.y = temp.y;
                        box.z = temp.z;
                        box.label = temp.label;
                        boxData_.Add(box);
                    }
                }
                else
                {
                    //boxMgr.CreateBoundingBoxObject (temp.position, temp.x, temp.y, temp.z, temp.label, c);
                    box.position = temp.position;
                    box.x = temp.x;
                    box.y = temp.y;
                    box.z = temp.z;
                    box.label = temp.label;
                    boxData_.Add(box);
                }
            }
        }
        catch (System.Exception e)
        {
            Debug.LogExceptionFormat(e, "while updating bounding boxes");
        }


        try
        {
            Debug.LogFormat("will create {0} new boxes", boxData_.Count);

            if (boxData_.Count > 0)
                CreateBoxes(boxData_);
        }
        catch (System.Exception e)
        {
            Debug.LogExceptionFormat(e, "while creating new bounding boxes");
        }
    }

    public void CreateBoxes(List<CreateBoxData> boxes)
    {
        //create bounding boxes
        Color c = colors_[UnityEngine.Random.Range(0, colors_.Count)];
        if (!renderBoundingBoxes)
            c.a = 0; //Make this box transparent
        for (int i = 0; i < boxes.Count; i++)
        {
            //Vector3 filteredPos = kalman.Update(boxes [i].position);
            if (!renderBoundingBoxes)
                boxMgr_.CreateBoundingBoxObject(boxes[i].position, boxes[i].x, boxes[i].y, boxes[i].z, "", c, false);
            //boxMgr.CreateBoundingBoxObject(boxes[i].position, boxes[i].x, boxes[i].y, boxes[i].z, "", c);
            else
                boxMgr_.CreateBoundingBoxObject(boxes[i].position, boxes[i].x, boxes[i].y, boxes[i].z, boxes[i].label, c);
        }
        boxData_.Clear();

        //initialize Kalman filters
        foreach (KeyValuePair<string, List<BoundingBox>> kvp in boxMgr_.boundingBoxObjects)
        {
            for (int i = 0; i < kvp.Value.Count; i++)
            {
                kalman_[kvp.Value[i].guid] = new MatrixKalmanWrapper();
            }
        }
        Debug.Log("Kalman filters: " + kalman_.Count);
    }

    public void UpdateBoxes()
    {
        foreach (KeyValuePair<string, List<BoundingBox>> kvp in boxMgr_.boundingBoxObjects)
        {
            for (int i = 0; i < kvp.Value.Count; i++)
            {
                Vector3 filteredPos = kalman_[kvp.Value[i].guid].Update(kvp.Value[i].last);
                boxMgr_.UpdateBoundingBoxObject(kvp.Value[i], filteredPos, kvp.Value[i].x, kvp.Value[i].y, kvp.Value[i].z, kvp.Value[i].label.labelText, filteredPos);
            }
        }
    }

    public void fetchModel(string modelId)
    {
        var modelName = "/icear/content-publisher/avatars/" + modelId + ".model";
        assetFetcher_.fetch(modelName, delegate (AssetBundle assetBundle)
        {
            Debug.Log("Fetched asset bundle...");
            // TODO: load asset bundle into the scene, cache it locally, etc...
        });
    }

    public void processDbQueryReply()
    {

    }

    public void OnImageAvailable(TextureReaderApi.ImageFormatType format, int width, int height, IntPtr pixelBuffer, int bufferSize)
    {
        try
        {
            System.DateTime current = System.DateTime.Now;
            long elapsedTicks = current.Ticks - begin_.Ticks;
            System.TimeSpan elapsedSpan = new System.TimeSpan(elapsedTicks);
            timestamp_ = elapsedSpan.TotalSeconds;

            Debug.LogFormat("pushing frame {0}x{1} to NDN-RTC...", width, height);

            FrameInfo finfo = NdnRtc.videoStream.processIncomingFrame(format, width, height, pixelBuffer, bufferSize);
            int publishedFrameNo = finfo.playbackNo_;

            if (publishedFrameNo >= 0)
            {
                Debug.LogFormat("create frame object #{0}, ts {1}, pos {2}, rot {3}, cam {4}",
                                publishedFrameNo, timestamp_, Frame.Pose.position, Frame.Pose.rotation, camForCalcThread_.ToString());

                frameMgr_.CreateFrameObject(publishedFrameNo, timestamp_, Frame.Pose.position, Frame.Pose.rotation, camForCalcThread_);
                frameBuffer_.Enqueue(frameMgr_.frameObjects);

                // spawn fetching task for annotations of this frame
                // once successfully received, delegate callback will be called
                yoloFetcher_.fetchAnnotation(publishedFrameNo, delegate (string jsonArrayString)
                {
                    int frameNumber = publishedFrameNo; // storing frame number locally
                    string debugString = jsonArrayString.Replace(System.Environment.NewLine, "");
                    var now = System.DateTime.Now;
                    bool runQuery = false;

                    Debug.LogFormat((ILogComponent)this, "fetched annotations JSON for {0}, length {1}: {2}", 
                                    frameNumber, jsonArrayString.Length, debugString);

                    string[] testDebug = jsonArrayString.Split(']');
                    string formatDebug = testDebug[0] + "]";
                
                    try
                    {
                        // check if it's time to query Semantic DB...
                        if ((float)(now - lastDbQuery_).TotalSeconds >= (1f / dbQueryRate_))
                        {
                            runQuery = true;
                            lastDbQuery_ = now;
                            dbController_.runQuery(jsonArrayString,
                                                   delegate (DbReply reply, string errorMessage)
                            {
                                if (reply != null)
                                {
                                    Debug.LogFormat(dbController_, "got reply from DB. entries {0} ", reply.entries.Length);
                                    foreach (var entry in reply.entries)
                                    {
                                        NdnRtc.fetch(entry.frameName, NdnRtc.videoStream,
                                        delegate (FrameInfo fi, int w, int h, byte[] argbBuffer)
                                        {
                                            Debug.LogFormat("[ff-task]: succesfully fetched frame {0}", fi.ndnName_);
                                            imageController_.enqueueFrame(new FetchedUIFrame(argbBuffer, fi.timestamp_, entry.simLevel));
                                        },
                                        delegate (string frameName)
                                        {
                                            Debug.LogFormat("[ff-task]: failed to fetch {0}", frameName);
                                        });
                                    }
                                }
                                else
                                {
                                    Debug.ErrorFormat(dbController_, "db request error {0} ", errorMessage);
                                }
                            });
                        } // if time for DB query

                        Dictionary<int, FrameObjectData> frameObjects = frameBuffer_.Dequeue();
                        FrameObjectData frameObjectData;

                        if (frameObjects.TryGetValue(frameNumber, out frameObjectData))
                        {
                            //AnnotationData[] data = JsonHelper.FromJson<AnnotationData>(jsonArrayString);
                            //try to print out how many characters the jsonArrayString has
                            string str = "{ \"annotationData\": " + formatDebug + "}";
                            AnnotationData data = JsonUtility.FromJson<AnnotationData>(str);

#if DEVELOPMENT_BUILD
                            for (int i = 0; i < data.annotationData.Length; i++)
                                Debug.LogFormat((ILogComponent)this,
                                                "annotation {0}: label {1} prob {2} xleft {3} xright {4} ytop {5} ybottom {6}",
                                                i, data.annotationData[i].label, data.annotationData[i].prob,
                                                data.annotationData[i].xleft, data.annotationData[i].xright,
                                                data.annotationData[i].ytop, data.annotationData[i].ybottom);
#endif

                            if (runQuery) //We only want to update our debug UI at (roughly) the query rate
                            {
                                lastKeyFrame_ = now;
                                imageController_.updateDebugText(data);
                            }

                            Debug.LogFormat((ILogComponent)this,
                                            "processing annotation for frame #{0}, cam pos {1}, cam rot {2}, points {3}, lifetime {4} sec",
                                            frameNumber, frameObjectData.camPos, frameObjectData.camRot, frameObjectData.points,
                                            Mathf.Abs((float)(frameObjectData.timestamp - timestamp_)));
                            
                            int boxCount = data.annotationData.Length;

                            // I believe the filtering based on annotations' probability must be done here, not in calcualtions function
                            BoxData annoData = new BoxData();

                            annoData.frameNumber = frameNumber;
                            annoData.count = boxCount;
                            annoData.points = frameObjectData.points;
                            annoData.numPoints = frameObjectData.numPoints;
                            annoData.cam = frameObjectData.cam;
                            annoData.camPos = frameObjectData.camPos;
                            annoData.camRot = frameObjectData.camRot;
                            annoData.timestamp = frameObjectData.timestamp;
                            annoData.label = new string[boxCount];
                            annoData.xleft = new float[boxCount];
                            annoData.xright = new float[boxCount];
                            annoData.ytop = new float[boxCount];
                            annoData.ybottom = new float[boxCount];
                            annoData.prob = new float[boxCount];

                            for (int i = 0; i < boxCount; i++)
                            {
                                annoData.label[i] = data.annotationData[i].label;
                                annoData.xleft[i] = 1 - data.annotationData[i].xright;
                                annoData.xright[i] = 1 - data.annotationData[i].xleft;
                                annoData.ytop[i] = data.annotationData[i].ybottom;
                                annoData.ybottom[i] = data.annotationData[i].ytop;
                                annoData.prob[i] = data.annotationData[i].prob;
                            }

                            //boxBufferToCalc.Enqueue(annoData);
                            boundingBoxBufferToCalc_.Enqueue(annoData);

                            Debug.Log((ILogComponent)this, "enqueued annotations data for processing");
                        }
                        else
                        {
                            //frame object was not in the pool, lifetime expired
                            Debug.Log((ILogComponent)this, "received annotations but frame expired");
                        }
                    }
                    catch (System.Exception e)
                    {
                        Debug.LogExceptionFormat(e, "while parsing annotation {0}...", debugString.Substring(0,100));
                    }
                });

                openFaceFetcher_.fetchAnnotation(publishedFrameNo, delegate (string jsonArrayString) {
                    int frameNumber = publishedFrameNo; // storing frame number locally
                    string debuglog = jsonArrayString.Replace(System.Environment.NewLine, " ");
                    Debug.Log("Received OpenFace annotations JSON (frame " + frameNumber + "): " + debuglog);
                    string[] testDebug = jsonArrayString.Split(']');
                    string formatDebug = testDebug[0] + "]";
                    try
                    {
                        Dictionary<int, FrameObjectData> frameObjects = frameBuffer_.Dequeue();
                        FrameObjectData temp;
                        if (frameObjects.TryGetValue(frameNumber, out temp))
                        {
                            string str = "{ \"annotationData\": " + formatDebug + "}";
                            AnnotationData data = JsonUtility.FromJson<AnnotationData>(str);
                            for (int i = 0; i < data.annotationData.Length; i++)
                            {
                                //if(data.annotationData[i].prob >= 0.7f)
                                {
                                    Debug.Log("openface test: " + data.annotationData.Length);
                                    Debug.Log("openface test label: " + data.annotationData[i].label + " test xleft: " + data.annotationData[i].xleft
                                        + " test xright: " + data.annotationData[i].xright + " test ytop: " + (data.annotationData[i].ytop) + " test ybottom: " + (data.annotationData[i].ybottom));

                                }
                            }
                            //int boxCount = Mathf.Min(data.annotationData.Length, 2);
                            int boxCount = data.annotationData.Length;

                            BoxData annoData = new BoxData();

                            annoData.frameNumber = frameNumber;
                            annoData.count = boxCount;
                            annoData.points = temp.points;
                            annoData.numPoints = temp.numPoints;
                            annoData.cam = temp.cam;
                            annoData.camPos = temp.camPos;
                            annoData.camRot = temp.camRot;
                            annoData.timestamp = temp.timestamp;
                            annoData.label = new string[boxCount];
                            annoData.xleft = new float[boxCount];
                            annoData.xright = new float[boxCount];
                            annoData.ytop = new float[boxCount];
                            annoData.ybottom = new float[boxCount];
                            annoData.prob = new float[boxCount];

                            for (int i = 0; i < boxCount; i++)
                            {
                                if (data.annotationData[i].ytop > 1)
                                    data.annotationData[i].ytop = 1;
                                if (data.annotationData[i].ybottom < 0)
                                    data.annotationData[i].ybottom = 0;
                                annoData.label[i] = data.annotationData[i].label;
                                annoData.xleft[i] = data.annotationData[i].xleft;
                                annoData.xright[i] = data.annotationData[i].xright;
                                annoData.ytop[i] = data.annotationData[i].ytop;
                                annoData.ybottom[i] = data.annotationData[i].ybottom;
                                annoData.prob[i] = 1;
                            }

                            Debug.Log("Received openface annotations box enqueue");
                            //boxBufferToCalc.Enqueue(annoData);
                            boundingBoxBufferToCalc_.Enqueue(annoData);
                        }
                        else
                        {
                            //frame object was not in the pool, lifetime expired
                            Debug.Log("Received openface annotations but frame expired");
                        }
                    }
                    catch (System.Exception e)
                    {
                        Debug.LogException(e);
                        Debug.Log("exception caught openface annotations: " + e);
                        string debug = jsonArrayString.Replace(System.Environment.NewLine, " ");
                        Debug.Log("exception caught openface string: " + debug);
                        string str = "{ \"annotationData\": " + debug + "}";
                        Debug.Log("exception caught openface string with format: " + str);
                    }
                });

            }
            else
            {
                // frame was dropped by the encoder and was not published
            }

        }
        catch (System.Exception e)
        {
            Debug.LogExceptionFormat(e, "in OnImageAvailable call");
        }

    }

    static void calculationsForBoundingBox()
    {
        //Debug.Log("started bounding boxes processing thread");
             
        //while (true)
        {

            try
            {
                //Thread.Sleep (2);
                //            BoxData temp;
                //            Debug.Log ("box before dequeue");
                //            bool success = boxBufferToCalc.TryDequeue (out temp);
                //            Debug.Log ("box dequeue: " + success);
                //            if (success) {
                Debug.LogFormat("checking bounding boxes for calculations... {0}", boundingBoxBufferToCalc_.Count);

                while (boundingBoxBufferToCalc_.Count > 0)
                {
                    BoxData frameBoxData = boundingBoxBufferToCalc_.Dequeue();
                    int boxCount = frameBoxData.count;

                    //Vector3[] min = new Vector3[boxCount];
                    float[] averageZ = new float[boxCount];
                    int[] numWithinBox = new int[boxCount];
                    List<float>[] pointsInBounds = new List<float>[boxCount];

                    for (int i = 0; i < boxCount; i++)
                    {
                        //min [i] = new Vector3 (100, 100, 100);
                        pointsInBounds[i] = new List<float>();
                        averageZ[i] = 0;
                        numWithinBox[i] = 0;
                    }

                    List<Vector4> frameCloudPoints = frameBoxData.points;

                    Debug.LogFormat("pointcloud points count {0}. applying transform", frameCloudPoints.Count);

                    frameBoxData.cam.transform.position = frameBoxData.camPos;
                    frameBoxData.cam.transform.rotation = frameBoxData.camRot;

                    Debug.LogFormat("bbox camera: pos {0} rot {1}, frame camera: pos {2}, rot {3}",
                                    frameBoxData.cam.transform.position.ToString(), frameBoxData.cam.transform.rotation.ToString(),
                                    frameBoxData.camPos.ToString(), frameBoxData.camRot.ToString());

                    Vector2[] centerPosXY = new Vector2[boxCount];
                    Vector2[] worldCenter = new Vector2[boxCount];
                    Vector3[] position = new Vector3[boxCount];

                    Vector2[] viewportTopLeft = new Vector2[boxCount];
                    Vector2[] viewportTopRight = new Vector2[boxCount];
                    Vector2[] viewportBottomLeft = new Vector2[boxCount];
                    Vector2[] viewportBottomRight = new Vector2[boxCount];

                    Vector3[] worldTopLeft = new Vector3[boxCount];
                    Vector3[] worldTopRight = new Vector3[boxCount];
                    Vector3[] worldBottomLeft = new Vector3[boxCount];
                    Vector3[] worldBottomRight = new Vector3[boxCount];

                    float[] x = new float[boxCount];
                    float[] y = new float[boxCount];
                    float[] z = new float[boxCount];


                    for (int i = 0; i < boxCount; i++)
                    {
                        //calucate 4 viewport corners
                        viewportTopLeft[i] = new Vector2(frameBoxData.xleft[i], frameBoxData.ytop[i]);
                        viewportTopRight[i] = new Vector2(frameBoxData.xright[i], frameBoxData.ytop[i]);
                        viewportBottomLeft[i] = new Vector2(frameBoxData.xleft[i], frameBoxData.ybottom[i]);
                        viewportBottomRight[i] = new Vector2(frameBoxData.xright[i], frameBoxData.ybottom[i]);


                        //calculate center of box in viewport coords
                        centerPosXY[i] = new Vector2(frameBoxData.xleft[i] + Mathf.Abs(viewportTopLeft[i].x - viewportTopRight[i].x) / 2,
                            frameBoxData.ybottom[i] + Mathf.Abs(viewportTopLeft[i].y - viewportBottomLeft[i].y) / 2);

                        Debug.LogFormat("bbox {0} topleft: {1} topright {2} botleft {3} botright {4} center {5}",
                                        i, 
                                        viewportTopLeft[i].ToString(), viewportTopRight[i].ToString(), 
                                        viewportBottomLeft[i].ToString(), viewportBottomRight[i].ToString(),
                                        centerPosXY[i].ToString());

                    }

                    try
                    {
                        // iterating all cloud points to place them within each bounding box
                        for (int i = 0; i < frameCloudPoints.Count; i++)
                        {
                            for (int j = 0; j < boxCount; j++)
                            {
                                //                        //calculate center of box in world coords
                                //                        worldCenter [j] = temp.cam.ViewportToWorldPoint (new Vector2 (centerPosXY [j].x, centerPosXY [j].y));
                                //                        //find point in points[] that most nearly matches center position
                                //                        if (Vector2.Distance (new Vector2 (points [i].x, points [i].y), worldCenter [j]) < Vector2.Distance (new Vector2 (min [j].x, min [j].y), worldCenter [j])) {
                                //                            min [j] = points [i];
                                //                        }
                                //find if points[i] is outside of the bounding box
                                Vector3 viewportPoint = frameBoxData.cam.WorldToViewportPoint(frameCloudPoints[i]);
                                if (viewportPoint.x < frameBoxData.xleft[j] || 
                                    viewportPoint.x > frameBoxData.xright[j] || 
                                    viewportPoint.y < frameBoxData.ybottom[j] ||
                                    viewportPoint.y > frameBoxData.ytop[j])
                                {
                                    //points[i] is out of the limits of the bounding box
                                }
                                else
                                {
                                    //points[i] is in the bounding box
                                    pointsInBounds[j].Add(frameCloudPoints[i].z);
                                    averageZ[j] += frameCloudPoints[i].z;
                                    numWithinBox[j]++;
                                }
                            }
                        }
                    }
                    catch (System.Exception e)
                    {
                        Debug.LogExceptionFormat(e, "while counting sorting points");
                    }

                    for (int i = 0; i < boxCount; i++)
                    {
                        float median;
                        float depth;

                        //  every list has just Z coordinate of a point, sort them
                        pointsInBounds[i].Sort();
                        //median = pointsInBounds [i][pointsInBounds[i].Count / 2];
                        //Debug.Log("median = " + median);
                        //averageZ [i] /= numWithinBox [i];

                        // if there are no points for particular box - it won't be rendered, apparently
                        if (!(pointsInBounds[i].Count == 0))
                        {
                            //float depth = Mathf.Abs(min [i].z);
                            Debug.LogFormat("median: float array len {0}, idx {1}", pointsInBounds[i].Count, pointsInBounds[i].Count / 2);

                            median = pointsInBounds[i][pointsInBounds[i].Count / 2];

                            Debug.LogFormat("median {0}", median);

                            depth = Mathf.Abs(median);
                            //float depth = Mathf.Abs (averageZ [i]);
                            if (depth < 0.5f)
                                depth = 0.5f;

                            //calculate center of box in world coords
                            // i believe this is not correct -- depth is a cloud point's Z location, i.e. in world coordiantes
                            // here, it's assigned as a Z component of a vector, which X and Y are in Viewport coordinate system
                            // it then gets transformed to the world coordinates again
                            // Z value shall be assigned after this transformation is done
                            position[i] = frameBoxData.cam.ViewportToWorldPoint(new Vector3(centerPosXY[i].x, centerPosXY[i].y, depth));

                            Debug.LogFormat("box {0}: position {1}", i, position[i].ToString());
                            //Debug.Log ("box: found min " + min.ToString ());

                            //calculate Z value for world corners
                            worldTopLeft[i] = frameBoxData.cam.ViewportToWorldPoint(new Vector3(viewportTopLeft[i].x, viewportTopLeft[i].y, depth));
                            worldTopRight[i] = frameBoxData.cam.ViewportToWorldPoint(new Vector3(viewportTopRight[i].x, viewportTopRight[i].y, depth));
                            worldBottomLeft[i] = frameBoxData.cam.ViewportToWorldPoint(new Vector3(viewportBottomLeft[i].x, viewportBottomLeft[i].y, depth));
                            worldBottomRight[i] = frameBoxData.cam.ViewportToWorldPoint(new Vector3(viewportBottomRight[i].x, viewportBottomRight[i].y, depth));


                            //calculate x, y, z size values
                            x[i] = Mathf.Abs(Vector3.Distance(worldTopLeft[i], worldTopRight[i]));
                            y[i] = Mathf.Abs(Vector3.Distance(worldTopLeft[i], worldBottomLeft[i]));
                            z[i] = 0; // why Z is zero here?

                            // why filtering is done so late here after all calculations are done?
                            // should filter out low prob annotations at the moment of parsing
                            if (frameBoxData.prob[i] >= 0.6f)
                            {
                                CreateBoxData boxData = new CreateBoxData();
                                boxData.label = frameBoxData.label[i];
                                boxData.position = position[i];
                                boxData.x = x[i];
                                boxData.y = y[i];
                                boxData.z = z[i];
                                boxData.cam = frameBoxData.cam;
                                boxData.frameNum = frameBoxData.frameNumber;
                                boxData.timestamp = frameBoxData.timestamp;
                                //boxBufferToUpdate.Enqueue (boxData);
                                boundingBoxBufferToUpdate_.Enqueue(boxData);
                            }

                        }
                    }
                }
            }
            catch (System.Exception e)
            {
                Debug.LogExceptionFormat(e, "while making bb calculations");
            }
        }
    }

    public string getLogComponentName()
    {
        return "on-camera-frame";
    }

    public bool isLoggingEnabled()
    {
#if ENABLE_LOG
        return true;
#else
        return false;
#endif
    }
}

[System.Serializable]
public struct AnnotationData
{
    [System.Serializable]
    public struct ArrayEntry
    {
        public float xleft;
        public float xright;
        public float ytop;
        public float ybottom;
        public string label;
        public float prob;
    }

    public ArrayEntry[] annotationData;
}

public struct CreateBoxData
{
    public Vector3 position;
    public float x;
    public float y;
    public float z;
    public int frameNum;
    public string label;
    public Camera cam;
    public double timestamp;
}

public struct BoxData
{
    public int frameNumber;
    public int count;
    public List<Vector4> points;
    public int numPoints;
    public Camera cam;
    public Vector3 camPos;
    public Quaternion camRot;
    public float[] xleft;
    public float[] xright;
    public float[] ytop;
    public float[] ybottom;
    public float[] prob;
    public string[] label;
    public double timestamp;
}