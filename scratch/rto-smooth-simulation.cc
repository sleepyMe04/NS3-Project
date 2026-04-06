/*
 * RTO simulation — single bottleneck topology (Xiao & Zhang 2015)
 *
 * HOW RTT/RTO LOGGING WORKS (the key fix):
 *
 *   We call RttMeanDeviation::SetLogStream() on each socket's estimator
 *   right after the socket is created. From that point, every single call
 *   to Measurement() inside the estimator writes ONE row containing:
 *
 *     Time | NodeId | FlowId | RTT | SRTT | RTTVAR | RTO
 *
 *   Because the row is written INSIDE Measurement() AFTER the EWMA update,
 *   the SRTT, RTTVAR, and RTO values are always perfectly consistent with
 *   the RTT sample in the same row. No merge_asof needed. No timing gaps.
 *
 *   This directly shows the algorithm difference:
 *     standard:  SRTT lags RTT, large RTTVAR, RTO >> RTT
 *     improved:  SRTT tracks RTT faster (high alpha), smaller RTTVAR
 *     elrto:     spikes suppressed → RTTVAR much smaller → RTO tighter
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "rtt-estimator.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RtoSmoothSimulation");

// Single output file shared across all flows in this run
static std::ofstream g_dataFile;

// ── Wire up the log stream on a specific node's socket ────────────
// Finds the RttMeanDeviation object inside the TcpSocketBase and
// calls SetLogStream() so it writes to g_dataFile.
static void
WireLogStreamOnNode(uint32_t nodeId, uint32_t flowId)
{
    Ptr<Node> node = NodeList::GetNode(nodeId);
    if (!node) { NS_LOG_WARN("Node " << nodeId << " not found"); return; }

    // Walk all objects aggregated to the node looking for TcpSocketBase
    // via the Config attribute system — most reliable in ns-3.45
    std::ostringstream rttEstPath;
    rttEstPath << "/NodeList/" << nodeId
               << "/$ns3::TcpL4Protocol/SocketList/*";

    // Use a lambda via Config::MatchContainer to visit each socket
    Config::MatchContainer mc =
        Config::LookupMatches(rttEstPath.str());

    bool found = false;
    for (std::size_t i = 0; i < mc.GetN(); ++i)
    {
        Ptr<Object> obj = mc.Get(i);
        Ptr<TcpSocketBase> sock = DynamicCast<TcpSocketBase>(obj);
        if (!sock) continue;

        // Get the RTT estimator from the socket
        // TcpSocketBase holds it as m_rtt (Ptr<RttEstimator>)
        // We access it through the object's TypeId attribute system
        // The attribute is named "RttEstimator" in ns-3.45
        PointerValue pv;
        sock->GetAttribute("RttEstimator", pv);
        Ptr<RttMeanDeviation> est =
            DynamicCast<RttMeanDeviation>(pv.Get<RttEstimator>());
        if (!est)
        {
            NS_LOG_WARN("Could not cast to RttMeanDeviation on node "
                        << nodeId);
            continue;
        }

        est->SetLogStream(&g_dataFile, nodeId, flowId);
        NS_LOG_INFO("Log stream wired: node=" << nodeId
                    << " flow=" << flowId
                    << " socket=" << i);
        found = true;
        break;  // one socket per sender node
    }

    if (!found)
        NS_LOG_WARN("No socket found on node " << nodeId
                    << " at t=" << Simulator::Now().GetSeconds());
}

int
main(int argc, char* argv[])
{
    uint32_t    nFlows          = 9;
    std::string accessRate      = "10Mbps";
    std::string accessDelay     = "2.5ms";
    std::string bottleneckRate  = "20Mbps";
    std::string bottleneckDelay = "5ms";
    uint32_t    packetSize      = 1000;
    uint32_t    queueSize       = 20;
    uint64_t    maxBytes        = 100000000;
    std::string cbrRate         = "1Mbps";
    bool        enableCbr       = true;
    double      simTime         = 30.0;

    std::string mode     = "standard";
    std::string scenario = "increase";
    uint32_t    elRtoWindow = 4;
    double      elRtoTheta  = 2.0;

    CommandLine cmd;
    cmd.AddValue("mode",        "Algorithm: standard|improved|elrto", mode);
    cmd.AddValue("scenario",    "Scenario: increase|decrease",        scenario);
    cmd.AddValue("simTime",     "Simulation time (s)",                simTime);
    cmd.AddValue("nFlows",      "Number of flows",                    nFlows);
    cmd.AddValue("enableCbr",   "Enable CBR background",              enableCbr);
    cmd.AddValue("cbrRate",     "CBR rate per flow",                  cbrRate);
    cmd.AddValue("elRtoWindow", "EL-RTO window M",                    elRtoWindow);
    cmd.AddValue("elRtoTheta",  "EL-RTO threshold theta",             elRtoTheta);
    cmd.Parse(argc, argv);

    if (scenario != "increase" && scenario != "decrease")
        NS_FATAL_ERROR("scenario must be: increase | decrease");
    if (mode != "standard" && mode != "improved" && mode != "elrto")
        NS_FATAL_ERROR("mode must be: standard | improved | elrto");

    // ── Algorithm configuration ────────────────────────────────────
    Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                       StringValue("ns3::TcpNewReno"));
    Config::SetDefault("ns3::RttMeanDeviation::Alpha", DoubleValue(0.125));
    Config::SetDefault("ns3::RttMeanDeviation::Beta",  DoubleValue(0.25));
    Config::SetDefault("ns3::TcpSocketBase::MinRto",   TimeValue(MilliSeconds(1)));
    Config::SetDefault("ns3::TcpSocket::SegmentSize",  UintegerValue(packetSize));

    if (mode == "standard")
    {
        Config::SetDefault("ns3::RttMeanDeviation::UseAdaptive",
                           BooleanValue(false));
        Config::SetDefault("ns3::RttMeanDeviation::UseElRto",
                           BooleanValue(false));
        NS_LOG_INFO("Mode: STANDARD — Jacobson RFC 6298");
    }
    else if (mode == "improved")
    {
        Config::SetDefault("ns3::RttMeanDeviation::UseAdaptive",
                           BooleanValue(true));
        Config::SetDefault("ns3::RttMeanDeviation::UseElRto",
                           BooleanValue(false));
        NS_LOG_INFO("Mode: IMPROVED — Xiao-Zhang adaptive alpha/beta");
    }
    else
    {
        Config::SetDefault("ns3::RttMeanDeviation::UseAdaptive",
                           BooleanValue(true));
        Config::SetDefault("ns3::RttMeanDeviation::UseElRto",
                           BooleanValue(true));
        Config::SetDefault("ns3::RttMeanDeviation::ElRtoWindow",
                           UintegerValue(elRtoWindow));
        Config::SetDefault("ns3::RttMeanDeviation::ElRtoTheta",
                           DoubleValue(elRtoTheta));
        NS_LOG_INFO("Mode: ELRTO — Xiao-Zhang + spike suppression");
    }

    LogComponentEnable("RtoSmoothSimulation", LOG_LEVEL_INFO);

    // ── Open unified output file ───────────────────────────────────
    std::string outName = mode + "_data_" + scenario + ".csv";
    g_dataFile.open(outName);
    NS_ASSERT_MSG(g_dataFile.is_open(), "Cannot open " + outName);
    g_dataFile << "Time,NodeId,FlowId,RTT(ms),SRTT(ms),RTTVAR(ms),RTO(ms)\n";

    // ── Topology ───────────────────────────────────────────────────
    NodeContainer senders, routers, receivers;
    senders.Create(nFlows);
    routers.Create(2);
    receivers.Create(nFlows);

    PointToPointHelper accessLink;
    accessLink.SetDeviceAttribute("DataRate", StringValue(accessRate));
    accessLink.SetChannelAttribute("Delay",   StringValue(accessDelay));
    accessLink.SetQueue("ns3::DropTailQueue", "MaxSize",
                        StringValue(std::to_string(queueSize) + "p"));

    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", StringValue(bottleneckRate));
    bottleneckLink.SetChannelAttribute("Delay",   StringValue(bottleneckDelay));
    bottleneckLink.SetQueue("ns3::DropTailQueue", "MaxSize",
                            StringValue(std::to_string(queueSize) + "p"));

    std::vector<NetDeviceContainer> senderDevs(nFlows);
    for (uint32_t i = 0; i < nFlows; ++i)
        senderDevs[i] = accessLink.Install(
            NodeContainer(senders.Get(i), routers.Get(0)));
    NetDeviceContainer bottleneckDevs = bottleneckLink.Install(routers);
    std::vector<NetDeviceContainer> receiverDevs(nFlows);
    for (uint32_t i = 0; i < nFlows; ++i)
        receiverDevs[i] = accessLink.Install(
            NodeContainer(routers.Get(1), receivers.Get(i)));

    InternetStackHelper stack;
    stack.Install(senders);
    stack.Install(routers);
    stack.Install(receivers);

    Ipv4AddressHelper address;
    for (uint32_t i = 0; i < nFlows; ++i)
    {
        std::ostringstream sub;
        sub << "10.1." << (i + 1) << ".0";
        address.SetBase(sub.str().c_str(), "255.255.255.0");
        address.Assign(senderDevs[i]);
    }
    address.SetBase("10.2.1.0", "255.255.255.0");
    address.Assign(bottleneckDevs);

    std::vector<Ipv4InterfaceContainer> rxIfaces(nFlows);
    for (uint32_t i = 0; i < nFlows; ++i)
    {
        std::ostringstream sub;
        sub << "10.3." << (i + 1) << ".0";
        address.SetBase(sub.str().c_str(), "255.255.255.0");
        rxIfaces[i] = address.Assign(receiverDevs[i]);
    }
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ── Applications ───────────────────────────────────────────────
    uint16_t tcpPort     = 9;
    uint16_t cbrPortBase = 10000;
    ApplicationContainer tcpSinkApps;

    for (uint32_t i = 0; i < nFlows; ++i)
    {
        Ipv4Address rxAddr = rxIfaces[i].GetAddress(1);
        Address sinkAddr(InetSocketAddress(rxAddr, tcpPort));

        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddr);
        ApplicationContainer sink = sinkHelper.Install(receivers.Get(i));
        sink.Start(Seconds(0.0));
        sink.Stop(Seconds(simTime));
        tcpSinkApps.Add(sink);

        BulkSendHelper srcHelper("ns3::TcpSocketFactory", sinkAddr);
        srcHelper.SetAttribute("SendSize", UintegerValue(packetSize));
        srcHelper.SetAttribute("MaxBytes", UintegerValue(maxBytes));
        ApplicationContainer src = srcHelper.Install(senders.Get(i));

        double startTime = 1.0;
        double stopTime  = simTime - 1.0;
        if (scenario == "increase")
            startTime = 1.0 + i * 2.0;
        else
            stopTime = std::max(1.1, simTime - 1.0 - i * 2.0);

        src.Start(Seconds(startTime));
        src.Stop(Seconds(stopTime));

        // Wire the log stream 5ms after start (socket exists by then)
        uint32_t nodeId = senders.Get(i)->GetId();
        uint32_t flowId = i;
        Simulator::Schedule(Seconds(startTime + 0.005),
                            &WireLogStreamOnNode, nodeId, flowId);

        if (enableCbr)
        {
            uint16_t cbrPort = cbrPortBase + i;
            PacketSinkHelper cbrSink(
                "ns3::UdpSocketFactory",
                InetSocketAddress(Ipv4Address::GetAny(), cbrPort));
            cbrSink.Install(receivers.Get(i)).Start(Seconds(0.0));

            OnOffHelper cbrSrc(
                "ns3::UdpSocketFactory",
                InetSocketAddress(rxAddr, cbrPort));
            cbrSrc.SetAttribute("DataRate",   StringValue(cbrRate));
            cbrSrc.SetAttribute("PacketSize", UintegerValue(packetSize));
            cbrSrc.SetAttribute("OnTime",
                StringValue("ns3::ConstantRandomVariable[Constant=1]"));
            cbrSrc.SetAttribute("OffTime",
                StringValue("ns3::ConstantRandomVariable[Constant=0]"));
            cbrSrc.SetAttribute("MaxBytes", UintegerValue(maxBytes));
            ApplicationContainer cbrSrc2 = cbrSrc.Install(senders.Get(i));
            cbrSrc2.Start(Seconds(startTime));
            cbrSrc2.Stop(Seconds(stopTime));
        }
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    uint64_t totalRxBytes = 0;
    for (uint32_t i = 0; i < tcpSinkApps.GetN(); ++i)
    {
        Ptr<PacketSink> s = DynamicCast<PacketSink>(tcpSinkApps.Get(i));
        if (s) totalRxBytes += s->GetTotalRx();
    }
    double throughputMbps = (totalRxBytes * 8.0) / (simTime * 1e6);

    g_dataFile.close();

    std::ofstream tpFile(mode + "_throughput_" + scenario + ".csv");
    tpFile << "Scenario,Algorithm,Throughput(Mbps)\n";
    tpFile << scenario << "," << mode << "," << throughputMbps << "\n";
    tpFile.close();

    Simulator::Destroy();

    NS_LOG_INFO("Done. Throughput=" << throughputMbps << " Mbps");
    NS_LOG_INFO("Saved: " << outName);
    return 0;
}


/*
./ns3 build -- -j1

./ns3 run "rto-smooth-simulation --mode=standard --scenario=increase"
./ns3 run "rto-smooth-simulation --mode=standard --scenario=decrease"
./ns3 run "rto-smooth-simulation --mode=improved --scenario=increase"
./ns3 run "rto-smooth-simulation --mode=improved --scenario=decrease"
./ns3 run "rto-smooth-simulation --mode=elrto    --scenario=increase"
./ns3 run "rto-smooth-simulation --mode=elrto    --scenario=decrease"

wc -l *_rtt_*.csv *_rto_*.csv   # verify data exists
python3 plot_results.py both


*/