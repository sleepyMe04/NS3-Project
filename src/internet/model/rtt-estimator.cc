//
// Copyright (c) 2006 Georgia Tech Research Corporation
// SPDX-License-Identifier: GPL-2.0-only
// Author: Rajib Bhattacharjea<raj.b@gatech.edu>
//
// MODIFIED: Added adaptive alpha/beta for improved RTO calculation
// Based on: "Improved RTO Algorithm for TCP Retransmission Timeout"
// by Xiao Jianliang and Zhang Kun

#include "rtt-estimator.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/log.h"

#include <cmath>
#include <iostream>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RttEstimator");

NS_OBJECT_ENSURE_REGISTERED(RttEstimator);

/// Tolerance used to check reciprocal of two numbers.
static const double TOLERANCE = 1e-6;

TypeId
RttEstimator::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RttEstimator")
                            .SetParent<Object>()
                            .SetGroupName("Internet")
                            .AddAttribute("InitialEstimation",
                                          "Initial RTT estimate",
                                          TimeValue(Seconds(1)),
                                          MakeTimeAccessor(&RttEstimator::m_initialEstimatedRtt),
                                          MakeTimeChecker());
    return tid;
}

Time
RttEstimator::GetEstimate() const
{
    return m_estimatedRtt;
}

Time
RttEstimator::GetVariation() const
{
    return m_estimatedVariation;
}

// Base class methods

RttEstimator::RttEstimator()
    : m_nSamples(0)
{
    NS_LOG_FUNCTION(this);

    // We need attributes initialized here, not later, so use the
    // ConstructSelf() technique documented in the manual
    ObjectBase::ConstructSelf(AttributeConstructionList());
    m_estimatedRtt = m_initialEstimatedRtt;
    m_estimatedVariation = Time(0);
    NS_LOG_DEBUG("Initialize m_estimatedRtt to " << m_estimatedRtt.GetSeconds() << " sec.");
}

RttEstimator::RttEstimator(const RttEstimator& c)
    : Object(c),
      m_initialEstimatedRtt(c.m_initialEstimatedRtt),
      m_estimatedRtt(c.m_estimatedRtt),
      m_estimatedVariation(c.m_estimatedVariation),
      m_nSamples(c.m_nSamples)
{
    NS_LOG_FUNCTION(this);
}

RttEstimator::~RttEstimator()
{
    NS_LOG_FUNCTION(this);
}

void
RttEstimator::Reset()
{
    NS_LOG_FUNCTION(this);
    // Reset to initial state
    m_estimatedRtt = m_initialEstimatedRtt;
    m_estimatedVariation = Time(0);
    m_nSamples = 0;
}

uint32_t
RttEstimator::GetNSamples() const
{
    return m_nSamples;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Mean-Deviation Estimator

NS_OBJECT_ENSURE_REGISTERED(RttMeanDeviation);

TypeId
RttMeanDeviation::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::RttMeanDeviation")
            .SetParent<RttEstimator>()
            .SetGroupName("Internet")
            .AddConstructor<RttMeanDeviation>()
            .AddAttribute("Alpha",
                          "Gain used in estimating the RTT, must be 0 <= alpha <= 1",
                          DoubleValue(0.125),
                          MakeDoubleAccessor(&RttMeanDeviation::m_alpha),
                          MakeDoubleChecker<double>(0, 1))
            .AddAttribute("Beta",
                          "Gain used in estimating the RTT variation, must be 0 <= beta <= 1",
                          DoubleValue(0.25),
                          MakeDoubleAccessor(&RttMeanDeviation::m_beta),
                          MakeDoubleChecker<double>(0, 1))
            .AddAttribute("UseAdaptive",
                          "Enable adaptive alpha/beta mechanism based on RTT change rate",
                          BooleanValue(false),
                          MakeBooleanAccessor(&RttMeanDeviation::m_useAdaptive),
                          MakeBooleanChecker())
            .AddAttribute("UseElRto",
                          "Enable EL-RTO spike suppression",
                          BooleanValue(false),
                          MakeBooleanAccessor(&RttMeanDeviation::m_useElRto),
                          MakeBooleanChecker())
            .AddAttribute("ElRtoWindow",
                          "EL-RTO sliding window size M",
                          UintegerValue(4),
                          MakeUintegerAccessor(&RttMeanDeviation::m_elRtoWindow),
                          MakeUintegerChecker<uint32_t>(2, 10))
            .AddAttribute("ElRtoTheta",
                          "EL-RTO spike threshold theta",
                          DoubleValue(2.0),
                          MakeDoubleAccessor(&RttMeanDeviation::m_elRtoTheta),
                          MakeDoubleChecker<double>(1.0, 5.0));
    return tid;
}

RttMeanDeviation::RttMeanDeviation()
    : m_previousRtt(Time(0)),
      m_alpha0(0.125),
      m_beta0(0.25),
      m_currentAlpha(0.125),
      m_currentBeta(0.25),
      m_useAdaptive(false),
      m_useElRto(false),
      m_elRtoWindow(4),
      m_elRtoTheta(2.0),
      m_logStream(nullptr),
      m_logNodeId(0),
      m_logFlowId(0)
{
    NS_LOG_FUNCTION(this);
}

RttMeanDeviation::RttMeanDeviation(const RttMeanDeviation& c)
    : RttEstimator(c),
      m_alpha(c.m_alpha),
      m_beta(c.m_beta),
      m_previousRtt(c.m_previousRtt),
      m_alpha0(c.m_alpha0),
      m_beta0(c.m_beta0),
      m_currentAlpha(c.m_currentAlpha),
      m_currentBeta(c.m_currentBeta),
      m_useAdaptive(c.m_useAdaptive),
      m_useElRto(c.m_useElRto),
      m_elRtoWindow(c.m_elRtoWindow),
      m_elRtoTheta(c.m_elRtoTheta),
      m_elRtoSamples(c.m_elRtoSamples),
      m_logStream(c.m_logStream),
      m_logNodeId(c.m_logNodeId),
      m_logFlowId(c.m_logFlowId)
{
    NS_LOG_FUNCTION(this);
}

uint32_t
RttMeanDeviation::CheckForReciprocalPowerOfTwo(double val) const
{
    NS_LOG_FUNCTION(this << val);
    if (val < TOLERANCE)
    {
        return 0;
    }
    // supports 1/32, 1/16, 1/8, 1/4, 1/2
    if (std::abs(1 / val - 8) < TOLERANCE)
    {
        return 3;
    }
    if (std::abs(1 / val - 4) < TOLERANCE)
    {
        return 2;
    }
    if (std::abs(1 / val - 32) < TOLERANCE)
    {
        return 5;
    }
    if (std::abs(1 / val - 16) < TOLERANCE)
    {
        return 4;
    }
    if (std::abs(1 / val - 2) < TOLERANCE)
    {
        return 1;
    }
    return 0;
}

double
RttMeanDeviation::CalculateChangeRate(Time currentRtt)
{
    NS_LOG_FUNCTION(this << currentRtt);
    
    // If this is the first or second measurement, no previous RTT exists
    if (m_previousRtt.GetSeconds() <= 0.0)
    {
        NS_LOG_DEBUG("No previous RTT, change rate k = 0");
        return 0.0;
    }
    
    double current = currentRtt.GetSeconds();
    double previous = m_previousRtt.GetSeconds();
    
    // Calculate k = |(RTT_n - RTT_{n-1}) / RTT_{n-1}|
    double k = std::abs((current - previous) / previous);
    
    // Cap k at 1.0 as per paper
    if (k > 1.0)
    {
        k = 1.0;
    }
    
    NS_LOG_DEBUG("RTT change rate k = " << k 
                 << " (current=" << current << "s, previous=" << previous << "s)");
    
    return k;
}

void
RttMeanDeviation::UpdateAdaptiveWeights(double k)
{
    NS_LOG_FUNCTION(this << k);
    
    // Update alpha: alpha_n = alpha_0 * (1 + k)
    m_currentAlpha = m_alpha0 * (1.0 + k);
    
    // Update beta: beta_n = beta_0 * (1 - k)
    m_currentBeta = m_beta0 * (1.0 - k);
    
    // Ensure weights stay in valid range [0, 1]
    if (m_currentAlpha > 1.0)
    {
        m_currentAlpha = 1.0;
    }
    if (m_currentBeta < 0.0)
    {
        m_currentBeta = 0.0;
    }
    
    NS_LOG_DEBUG("Adaptive weights: alpha = " << m_currentAlpha 
                 << ", beta = " << m_currentBeta);
}

Time
RttMeanDeviation::ApplySpikeSuppression(Time rawRtt)
{
    NS_LOG_FUNCTION(this << rawRtt);
    
    // Add current sample to window
    m_elRtoSamples.push_back(rawRtt);
    
    // Maintain window size
    if (m_elRtoSamples.size() > m_elRtoWindow)
    {
        m_elRtoSamples.pop_front();
    }
    
    // Need at least 2 samples to compute mean
    if (m_elRtoSamples.size() < 2)
    {
        return rawRtt;  // Pass through
    }
    
    // Compute window mean S
    int64_t sum = 0;
    for (const Time& t : m_elRtoSamples)
    {
        sum += t.GetInteger();
    }
    Time S = Time::From(sum / static_cast<int64_t>(m_elRtoSamples.size()));
    
    // Apply spike threshold
    Time threshold = Time::From(static_cast<int64_t>(S.GetInteger() * m_elRtoTheta));
    
    if (rawRtt > threshold)
    {
        NS_LOG_DEBUG("Spike detected: RTT=" << rawRtt.GetMilliSeconds()
                     << "ms > theta*S=" << threshold.GetMilliSeconds()
                     << "ms, clamping to S=" << S.GetMilliSeconds() << "ms");
        return S;  // Suppress spike
    }
    
    return rawRtt;  // Pass through
}

void
RttMeanDeviation::FloatingPointUpdate(Time m)
{
    NS_LOG_FUNCTION(this << m);

    // Use adaptive alpha/beta if enabled
    double alpha = m_useAdaptive ? m_currentAlpha : m_alpha;
    double beta = m_useAdaptive ? m_currentBeta : m_beta;
    
    NS_LOG_DEBUG("Using alpha = " << alpha << ", beta = " << beta 
                 << " (adaptive=" << m_useAdaptive << ")");

    // EWMA formulas are implemented as suggested in
    // Jacobson/Karels paper appendix A.2

    // SRTT <- (1 - alpha) * SRTT + alpha * R'
    Time err(m - m_estimatedRtt);
    double gErr = err.ToDouble(Time::S) * alpha;
    m_estimatedRtt += Time::FromDouble(gErr, Time::S);

    // RTTVAR <- (1 - beta) * RTTVAR + beta * |SRTT - R'|
    Time difference = Abs(err) - m_estimatedVariation;
    m_estimatedVariation += difference * beta;
    
    NS_LOG_DEBUG("Updated SRTT = " << m_estimatedRtt.GetSeconds() 
                 << "s, RTTVAR = " << m_estimatedVariation.GetSeconds() << "s");
}

void
RttMeanDeviation::IntegerUpdate(Time m, uint32_t rttShift, uint32_t variationShift)
{
    NS_LOG_FUNCTION(this << m << rttShift << variationShift);
    
    // NOTE: Integer update uses fixed alpha/beta for performance
    // Adaptive mechanism uses floating point update
    
    // Jacobson/Karels paper appendix A.2
    int64_t meas = m.GetInteger();
    int64_t delta = meas - m_estimatedRtt.GetInteger();
    int64_t srtt = (m_estimatedRtt.GetInteger() << rttShift) + delta;
    m_estimatedRtt = Time::From(srtt >> rttShift);
    if (delta < 0)
    {
        delta = -delta;
    }
    delta -= m_estimatedVariation.GetInteger();
    int64_t rttvar = m_estimatedVariation.GetInteger() << variationShift;
    rttvar += delta;
    m_estimatedVariation = Time::From(rttvar >> variationShift);
}

void
RttMeanDeviation::SetLogStream(std::ofstream* logStream, 
                                uint32_t nodeId, 
                                uint32_t flowId)
{
    NS_LOG_FUNCTION(this << logStream << nodeId << flowId);
    m_logStream = logStream;
    m_logNodeId = nodeId;
    m_logFlowId = flowId;
}

void
RttMeanDeviation::Measurement(Time m)
{
    NS_LOG_FUNCTION(this << m);
    
    if (m_nSamples)
    {
        // Calculate adaptive weights if enabled
        if (m_useAdaptive)
        {
            double k = CalculateChangeRate(m);
            UpdateAdaptiveWeights(k);
        }
        
        // Apply EL-RTO spike suppression if enabled
        Time processedRtt = m;
        if (m_useElRto)
        {
            processedRtt = ApplySpikeSuppression(m);
        }
        
        // Always use floating point update when adaptive or EL-RTO is enabled
        if (m_useAdaptive || m_useElRto)
        {
            FloatingPointUpdate(processedRtt);
        }
        else
        {
            // Original behavior: use integer update if possible
            uint32_t rttShift = CheckForReciprocalPowerOfTwo(m_alpha);
            uint32_t variationShift = CheckForReciprocalPowerOfTwo(m_beta);
            if (rttShift && variationShift)
            {
                IntegerUpdate(processedRtt, rttShift, variationShift);
            }
            else
            {
                FloatingPointUpdate(processedRtt);
            }
        }
    }
    else
    {
        // First sample
        m_estimatedRtt = m;
        m_estimatedVariation = m / 2;
        NS_LOG_DEBUG("(first sample) m_estimatedRtt = " << m_estimatedRtt.GetSeconds() 
                     << "s, m_estimatedVariation = " << m_estimatedVariation.GetSeconds() << "s");
    }
    
    // Store current RTT for next calculation
    m_previousRtt = m;
    m_nSamples++;
    
    // ── UNIFIED LOGGING ────────────────────────────────────────────
    // Write one complete row AFTER all updates are done.
    // This ensures RTT, SRTT, RTTVAR, RTO are perfectly synchronized.
    if (m_logStream && m_logStream->is_open())
    {
        // Calculate RTO = SRTT + 4*RTTVAR (standard formula)
        Time rto = m_estimatedRtt + Time::From(m_estimatedVariation.GetInteger() * 4);
        
        *m_logStream << Simulator::Now().GetSeconds() << ","
                     << m_logNodeId << ","
                     << m_logFlowId << ","
                     << m.GetMilliSeconds() << ","                      // RTT (raw)
                     << m_estimatedRtt.GetMilliSeconds() << ","         // SRTT
                     << m_estimatedVariation.GetMilliSeconds() << ","   // RTTVAR
                     << rto.GetMilliSeconds() << "\n";                  // RTO
    }
}

Ptr<RttEstimator>
RttMeanDeviation::Copy() const
{
    NS_LOG_FUNCTION(this);
    return CopyObject<RttMeanDeviation>(this);
}

void
RttMeanDeviation::Reset()
{
    NS_LOG_FUNCTION(this);
    RttEstimator::Reset();
    
    // Reset adaptive mechanism state
    m_previousRtt = Time(0);
    m_currentAlpha = m_alpha0;
    m_currentBeta = m_beta0;
    
    // Reset EL-RTO state
    m_elRtoSamples.clear();
    
    // Don't reset m_logStream, m_logNodeId, m_logFlowId
    // They should persist across resets
}

} // namespace ns3