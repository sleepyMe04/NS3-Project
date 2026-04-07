#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/rtt-estimator.h"

#include <fstream>
#include <sstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RtoElrtoSpikeSimulation");

static std::ofstream g_dataFile;

static void
WireLogStreamOnNode(uint32_t nodeId, uint32_t flowId)
{
    std::ostringstream path;
    path << "/NodeList/" << nodeId << "/$ns3::TcpL4Protocol/SocketList/*";

    Config::MatchContainer mc = Config::LookupMatches(path.str());
    for (std::size_t i = 0; i < mc.GetN(); ++i)
    {
        Ptr<Object> obj = mc.Get(i);
        Ptr<TcpSocketBase> sock = DynamicCast<TcpSocketBase>(obj);
        if (!sock)
        {
            continue;
        }

        Ptr<RttMeanDeviation> est = DynamicCast<RttMeanDeviation>(sock->GetRtt());
        if (!est)
        {
            continue;
        }

        est->SetLogStream(&g_dataFile, nodeId, flowId);
        NS_LOG_INFO("Wired estimator logging on node " << nodeId << " flow " << flowId);
        return;
    }

    NS_LOG_WARN("No TCP socket found on node " << nodeId << " for logging");
}

int
main(int argc, char* argv[])
{
    std::string mode = "standard";
    std::string accessRate = "10Mbps";
    std::string accessDelay = "2.5ms";
    std::string bottleneckRate = "5Mbps";
    std::string bottleneckDelay = "5ms";
    uint32_t packetSize = 1000;
    uint32_t queueSize = 20;
    double simTime = 20.0;
    double tcpStart = 1.0;
    double udpSpikeStart = 10.0;
    double udpSpikeDuration = 0.5;
    std::string udpSpikeRate = "30Mbps";
    uint32_t elRtoWindow = 4;
    double elRtoTheta = 2.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("mode", "standard|improved|elrto", mode);
    cmd.AddValue("accessRate", "Access link rate", accessRate);
    cmd.AddValue("accessDelay", "Access link delay", accessDelay);
    cmd.AddValue("bottleneckRate", "Bottleneck link rate", bottleneckRate);
    cmd.AddValue("bottleneckDelay", "Bottleneck link delay", bottleneckDelay);
    cmd.AddValue("packetSize", "TCP and UDP packet size", packetSize);
    cmd.AddValue("queueSize", "DropTail queue size in packets", queueSize);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("tcpStart", "TCP flow start time", tcpStart);
    cmd.AddValue("udpSpikeStart", "UDP burst start time", udpSpikeStart);
    cmd.AddValue("udpSpikeDuration", "UDP burst duration", udpSpikeDuration);
    cmd.AddValue("udpSpikeRate", "UDP burst offered rate", udpSpikeRate);
    cmd.AddValue("elRtoWindow", "EL-RTO window M", elRtoWindow);
    cmd.AddValue("elRtoTheta", "EL-RTO threshold theta", elRtoTheta);
    cmd.Parse(argc, argv);

    if (mode != "standard" && mode != "improved" && mode != "elrto")
    {
        NS_FATAL_ERROR("mode must be standard, improved, or elrto");
    }

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));
    Config::SetDefault("ns3::RttMeanDeviation::Alpha", DoubleValue(0.125));
    Config::SetDefault("ns3::RttMeanDeviation::Beta", DoubleValue(0.25));
    Config::SetDefault("ns3::TcpSocketBase::MinRto", TimeValue(MilliSeconds(1)));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(packetSize));

    if (mode == "standard")
    {
        Config::SetDefault("ns3::RttMeanDeviation::UseAdaptive", BooleanValue(false));
        Config::SetDefault("ns3::RttMeanDeviation::UseElRto", BooleanValue(false));
    }
    else if (mode == "improved")
    {
        Config::SetDefault("ns3::RttMeanDeviation::UseAdaptive", BooleanValue(true));
        Config::SetDefault("ns3::RttMeanDeviation::UseElRto", BooleanValue(false));
    }
    else
    {
        Config::SetDefault("ns3::RttMeanDeviation::UseAdaptive", BooleanValue(true));
        Config::SetDefault("ns3::RttMeanDeviation::UseElRto", BooleanValue(true));
        Config::SetDefault("ns3::RttMeanDeviation::ElRtoWindow", UintegerValue(elRtoWindow));
        Config::SetDefault("ns3::RttMeanDeviation::ElRtoTheta", DoubleValue(elRtoTheta));
    }

    std::string outName = mode + "_spike_data.csv";
    g_dataFile.open(outName);
    NS_ASSERT_MSG(g_dataFile.is_open(), "Cannot open " + outName);
    g_dataFile << "Time,NodeId,FlowId,RTT(ms),SRTT(ms),RTTVAR(ms),RTO(ms)\n";

    NodeContainer nodes;
    nodes.Create(6);

    Ptr<Node> tcpSender = nodes.Get(0);
    Ptr<Node> tcpReceiver = nodes.Get(1);
    Ptr<Node> leftRouter = nodes.Get(2);
    Ptr<Node> rightRouter = nodes.Get(3);
    Ptr<Node> udpSpikeSender = nodes.Get(4);
    Ptr<Node> udpSpikeReceiver = nodes.Get(5);

    PointToPointHelper accessLink;
    accessLink.SetDeviceAttribute("DataRate", StringValue(accessRate));
    accessLink.SetChannelAttribute("Delay", StringValue(accessDelay));
    accessLink.SetQueue("ns3::DropTailQueue",
                        "MaxSize",
                        StringValue(std::to_string(queueSize) + "p"));

    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", StringValue(bottleneckRate));
    bottleneckLink.SetChannelAttribute("Delay", StringValue(bottleneckDelay));
    bottleneckLink.SetQueue("ns3::DropTailQueue",
                            "MaxSize",
                            StringValue(std::to_string(queueSize) + "p"));

    NetDeviceContainer dTcpLeft = accessLink.Install(NodeContainer(tcpSender, leftRouter));
    NetDeviceContainer dUdpLeft = accessLink.Install(NodeContainer(udpSpikeSender, leftRouter));
    NetDeviceContainer dBottleneck = bottleneckLink.Install(NodeContainer(leftRouter, rightRouter));
    NetDeviceContainer dTcpRight = accessLink.Install(NodeContainer(rightRouter, tcpReceiver));
    NetDeviceContainer dUdpRight = accessLink.Install(NodeContainer(rightRouter, udpSpikeReceiver));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iTcpLeft = address.Assign(dTcpLeft);
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer iUdpLeft = address.Assign(dUdpLeft);
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer iBottleneck = address.Assign(dBottleneck);
    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer iTcpRight = address.Assign(dTcpRight);
    address.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer iUdpRight = address.Assign(dUdpRight);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t tcpPort = 5000;
    Address tcpSinkAddr(InetSocketAddress(iTcpRight.GetAddress(1), tcpPort));
    PacketSinkHelper tcpSink("ns3::TcpSocketFactory", tcpSinkAddr);
    ApplicationContainer tcpSinkApp = tcpSink.Install(tcpReceiver);
    tcpSinkApp.Start(Seconds(0.0));
    tcpSinkApp.Stop(Seconds(simTime));

    BulkSendHelper tcpSource("ns3::TcpSocketFactory", tcpSinkAddr);
    tcpSource.SetAttribute("SendSize", UintegerValue(packetSize));
    tcpSource.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer tcpSourceApp = tcpSource.Install(tcpSender);
    tcpSourceApp.Start(Seconds(tcpStart));
    tcpSourceApp.Stop(Seconds(simTime - 1.0));

    Simulator::Schedule(Seconds(tcpStart + 0.01), &WireLogStreamOnNode, tcpSender->GetId(), 0);

    uint16_t udpPort = 6000;
    PacketSinkHelper udpSink("ns3::UdpSocketFactory",
                             InetSocketAddress(Ipv4Address::GetAny(), udpPort));
    ApplicationContainer udpSinkApp = udpSink.Install(udpSpikeReceiver);
    udpSinkApp.Start(Seconds(0.0));
    udpSinkApp.Stop(Seconds(simTime));

    OnOffHelper udpSource("ns3::UdpSocketFactory",
                          InetSocketAddress(iUdpRight.GetAddress(1), udpPort));
    udpSource.SetAttribute("DataRate", StringValue(udpSpikeRate));
    udpSource.SetAttribute("PacketSize", UintegerValue(packetSize));
    udpSource.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    udpSource.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer udpSourceApp = udpSource.Install(udpSpikeSender);
    udpSourceApp.Start(Seconds(udpSpikeStart));
    udpSourceApp.Stop(Seconds(udpSpikeStart + udpSpikeDuration));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    Ptr<PacketSink> tcpRx = DynamicCast<PacketSink>(tcpSinkApp.Get(0));
    double throughputMbps = (tcpRx->GetTotalRx() * 8.0) / (simTime * 1e6);

    g_dataFile.close();

    std::ofstream tpFile(mode + "_spike_throughput.csv");
    tpFile << "mode,throughputMbps\n";
    tpFile << mode << "," << throughputMbps << "\n";
    tpFile.close();

    Simulator::Destroy();

    std::cout << "Saved " << outName << "\n";
    std::cout << "Throughput(Mbps)=" << throughputMbps << "\n";
    return 0;
}
