// 
// pipeline-control.cpp
//
//  Created by Peter Gusev on 10 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "pipeline-control.hpp"
#include <boost/assign.hpp>

#include "interest-control.hpp"
#include "pipeline-control-state-machine.hpp"
#include "frame-data.hpp"

using namespace ndnrtc;

//******************************************************************************
PipelineControl PipelineControl::defaultPipelineControl(const ndn::Name& threadPrefix,
            const boost::shared_ptr<IBuffer> buffer,
			const boost::shared_ptr<IPipeliner> pipeliner,
			const boost::shared_ptr<IInterestControl> interestControl,
			const boost::shared_ptr<ILatencyControl> latencyControl,
			const boost::shared_ptr<IPlayoutControl> playoutControl,
            const boost::shared_ptr<statistics::StatisticsStorage>& storage)
{
	PipelineControlStateMachine::Struct ctrl(threadPrefix);
    ctrl.buffer_ = buffer;
    ctrl.pipeliner_ = pipeliner;
	ctrl.interestControl_ = interestControl;
	ctrl.latencyControl_ = latencyControl;
	ctrl.playoutControl_ = playoutControl;
    ctrl.sstorage_ = storage;
	return PipelineControl(PipelineControlStateMachine::defaultStateMachine(ctrl),
		interestControl, pipeliner);
}

PipelineControl PipelineControl::videoPipelineControl(const ndn::Name& threadPrefix,
			const boost::shared_ptr<IBuffer> buffer,
            const boost::shared_ptr<IPipeliner> pipeliner,
			const boost::shared_ptr<IInterestControl> interestControl,
			const boost::shared_ptr<ILatencyControl> latencyControl,
			const boost::shared_ptr<IPlayoutControl> playoutControl,
            const boost::shared_ptr<statistics::StatisticsStorage>& storage)
{
	PipelineControlStateMachine::Struct ctrl(threadPrefix);
    ctrl.buffer_ = buffer;
	ctrl.pipeliner_ = pipeliner;
	ctrl.interestControl_ = interestControl;
	ctrl.latencyControl_ = latencyControl;
	ctrl.playoutControl_ = playoutControl;
    ctrl.sstorage_ = storage;
	return PipelineControl(PipelineControlStateMachine::videoStateMachine(ctrl),
		interestControl, pipeliner);
}

//******************************************************************************
PipelineControl::PipelineControl(const PipelineControlStateMachine& machine,
			const boost::shared_ptr<IInterestControl>& interestControl,
			const boost::shared_ptr<IPipeliner> pipeliner):
machine_(machine),
interestControl_(interestControl),
pipeliner_(pipeliner),
sampleLatch_({0,0})
{
	// machine_.attach(this);
	description_ = "pipeline-control";
}

PipelineControl::~PipelineControl()
{
	// machine_.detach(this);
}

void 
PipelineControl::start()
{
	if (machine_.getState() != kStateIdle)
		throw std::runtime_error("Can't start Pipeline Control as it has been "
			"started already. Use reset() and start() to restart.");

	sampleLatch_.delta_ = pipeliner_->getSequenceNumber(SampleClass::Delta);
	sampleLatch_.key_ = pipeliner_->getSequenceNumber(SampleClass::Delta);
	machine_.dispatch(boost::make_shared<PipelineControlEvent>(PipelineControlEvent::Start));

	LogDebugC << "started. samples latched at " 
	<< sampleLatch_.delta_ << "d & " 
	<< sampleLatch_.key_ << "k" << std::endl;
}

void 
PipelineControl::stop()
{
	machine_.dispatch(boost::make_shared<PipelineControlEvent>(PipelineControlEvent::Reset));
	LogDebugC << "stopped" << std::endl;
}

void 
PipelineControl::segmentArrived(const boost::shared_ptr<WireSegment>& s)
{
    if (s->getSampleClass() == SampleClass::Key ||
        s->getSampleClass() == SampleClass::Delta)
    {
    	if (passesBarrier(s->getInfo()))
        	machine_.dispatch(boost::make_shared<EventSegment>(s));
    }
}

void 
PipelineControl::segmentRequestTimeout(const NamespaceInfo& n)
{
	if (passesBarrier(n))
		machine_.dispatch(boost::make_shared<EventTimeout>(n));
}

void 
PipelineControl::segmentNack(const NamespaceInfo& n, int reason)
{
	if (passesBarrier(n))
		machine_.dispatch(boost::make_shared<EventNack>(n, reason));
}

void 
PipelineControl::segmentStarvation()
{
	machine_.dispatch(boost::make_shared<EventStarvation>(500));
    machine_.dispatch(boost::make_shared<PipelineControlEvent>(PipelineControlEvent::Start));
}

bool 
PipelineControl::needPipelineAdjustment(const PipelineAdjust& cmd)
{
	if (cmd == PipelineAdjust::IncreasePipeline ||
		cmd == PipelineAdjust::DecreasePipeline)
	{
		if (cmd == PipelineAdjust::IncreasePipeline) interestControl_->burst();
		if (cmd == PipelineAdjust::DecreasePipeline) interestControl_->withhold();
		return true;
	}
	return false;
}

void
PipelineControl::setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger)
{
    NdnRtcComponent::setLogger(logger);
    machine_.setLogger(logger);
}

#pragma mark - private
// void 
// PipelineControl::onStateMachineChangedState(const boost::shared_ptr<const PipelineControlEvent>& trigger,
// 			std::string newState)
// {
// 	// if new state is idle - reset the machine
// 	if (newState == kStateIdle && 
// 		trigger->getType() != PipelineControlEvent::Type::Reset)
// 	{
// 		LogInfoC << "state machine reverted to Idle. starting over..." << std::endl;
// 		start();
// 	}
// }

bool 
PipelineControl::passesBarrier(const NamespaceInfo& n)
{
	if (n.hasSeqNo_)
	{
		PacketNumber b = (n.class_ == SampleClass::Key ? sampleLatch_.key_ : sampleLatch_.delta_);
    	if (n.sampleNo_ >= b)
    		return true;
    	else
    		LogWarnC << "received sample number " << n.sampleNo_ 
        	<< " (" << n.getSuffix(suffix_filter::Stream) << ")"
        	<< " but samples latched at " << b << std::endl;

        return false;
    }
	
	return true;
}