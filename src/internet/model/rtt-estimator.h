//
// Copyright (c) 2006 Georgia Tech Research Corporation
// SPDX-License-Identifier: GPL-2.0-only
// Author: Rajib Bhattacharjea <raj.b@gatech.edu>
//
// MODIFIED:
//   [1] Adaptive alpha/beta based on RTT rate-of-change k
//       Xiao Jianliang & Zhang Kun, AMEII 2015 (base paper)
//   [2] EL-RTO spike suppression variable S
//       M. Joseph Auxilius Jude et al., Wireless Networks 2022

#ifndef RTT_ESTIMATOR_H
#define RTT_ESTIMATOR_H

#include "ns3/nstime.h"
#include "ns3/ptr.h"
#include "ns3/object.h"

#include <deque>
#include <fstream>

namespace ns3
{

/**
 * @ingroup tcp
 * @brief Base class for all RTT Estimators
 */
class RttEstimator : public Object
{
  public:
    static TypeId GetTypeId();

    RttEstimator();
    RttEstimator(const RttEstimator& r);
    ~RttEstimator() override;

    virtual void Measurement(Time t) = 0;
    virtual Ptr<RttEstimator> Copy() const = 0;
    virtual void Reset();

    Time     GetEstimate()  const;
    Time     GetVariation() const;
    uint32_t GetNSamples()  const;

  private:
    Time m_initialEstimatedRtt;

  protected:
    Time     m_estimatedRtt;
    Time     m_estimatedVariation;
    uint32_t m_nSamples;
};

/**
 * @ingroup tcp
 * @brief Mean-Deviation RTT estimator (Jacobson) with two optional extensions
 *
 * === UseAdaptive  [Xiao & Zhang, AMEII 2015] ===
 *   Computes RTT rate-of-change k = |RTT_n - RTT_{n-1}| / RTT_{n-1}
 *   and adjusts weights each sample:
 *       alpha_n = alpha0 * (1 + k)
 *       beta_n  = beta0  * (1 - k)
 *   so SRTT converges faster when RTT changes rapidly.
 *
 * === UseElRto  [Jude et al., Wireless Networks 2022] ===
 *   Maintains a sliding window of M recent raw RTT samples.
 *   Computes a spike suppression baseline:
 *       S_n = (1/M) * sum_{i=n-M+1}^{n}  RTT_i
 *   Before updating SRTT / RTTVAR, if the incoming RTT exceeds
 *   theta * S_n (a spike), the raw sample is clamped to S_n:
 *       RTT'_{n+1} = (RTT_{n+1} > theta*S_n) ? S_n : RTT_{n+1}
 *   SRTT and RTTVAR are then computed with RTT' instead of RTT.
 *   This prevents a single delayed-ACK event from over-inflating
 *   RTTVAR and therefore RTO, which is the primary failure mode
 *   in vehicular / wireless multi-hop networks.
 *
 * Both extensions can be combined or used independently.
 */
class RttMeanDeviation : public RttEstimator
{
  public:
    static TypeId GetTypeId();

    RttMeanDeviation();
    RttMeanDeviation(const RttMeanDeviation& r);

    void Measurement(Time measure) override;
    Ptr<RttEstimator> Copy() const override;
    void Reset() override;

    /**
     * @brief Enable unified logging of RTT, SRTT, RTTVAR, RTO after each measurement.
     *
     * When set, every call to Measurement() will write one CSV row:
     *   Time,NodeId,FlowId,RTT(ms),SRTT(ms),RTTVAR(ms),RTO(ms)
     *
     * This ensures perfect synchronization - all values in one row are
     * from the same EWMA update cycle. No merge_asof needed!
     *
     * @param logStream  Output file stream (must be open)
     * @param nodeId     Node identifier for this socket
     * @param flowId     Flow identifier for this socket
     */
    void SetLogStream(std::ofstream* logStream, uint32_t nodeId, uint32_t flowId);

  private:
    // ── Jacobson helpers ─────────────────────────────────────────
    uint32_t CheckForReciprocalPowerOfTwo(double val) const;
    void IntegerUpdate(Time m, uint32_t rttShift, uint32_t variationShift);
    void FloatingPointUpdate(Time m);

    // ── Xiao-Zhang 2015 helpers ──────────────────────────────────
    double CalculateChangeRate(Time currentRtt);
    void   UpdateAdaptiveWeights(double k);

    // ── EL-RTO 2022 helpers ──────────────────────────────────────
    /**
     * @brief Maintain the M-sample window and return the spike-suppressed RTT.
     *
     * Steps performed:
     *  1. Add rawRtt to the sliding window; evict oldest if window full.
     *  2. Compute window mean S.
     *  3. If rawRtt > theta * S  → return S  (spike suppressed)
     *     Else                   → return rawRtt (pass-through)
     *
     * @param rawRtt  RTT sample straight from the network stack
     * @return        RTT' value to feed into the EWMA update
     */
    Time ApplySpikeSuppression(Time rawRtt);

    // ── Attributes ───────────────────────────────────────────────
    double m_alpha;  //!< Base EWMA gain for SRTT   (default 0.125)
    double m_beta;   //!< Base EWMA gain for RTTVAR (default 0.25)

    // Xiao-Zhang adaptive state
    Time   m_previousRtt;
    double m_alpha0;
    double m_beta0;
    double m_currentAlpha;
    double m_currentBeta;
    bool   m_useAdaptive;   //!< Attribute: enable Xiao-Zhang adaptive weights

    // EL-RTO state
    bool             m_useElRto;      //!< Attribute: enable EL-RTO spike suppression
    uint32_t         m_elRtoWindow;   //!< Attribute: window size M (default 4)
    double           m_elRtoTheta;    //!< Attribute: spike threshold θ (default 2.0)
    std::deque<Time> m_elRtoSamples;  //!< Sliding window of recent raw RTT samples

    // Unified logging state
    std::ofstream* m_logStream;  //!< Optional log file stream
    uint32_t       m_logNodeId;  //!< Node ID for logging
    uint32_t       m_logFlowId;  //!< Flow ID for logging
};

} // namespace ns3

#endif /* RTT_ESTIMATOR_H */