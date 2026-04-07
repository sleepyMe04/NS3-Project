#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/energy-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/wifi-module.h"

#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <string>
#include <vector>

using namespace ns3;
using namespace ns3::lrwpan;

NS_LOG_COMPONENT_DEFINE("RtoChecklistWireless");

namespace
{

struct Metrics
{
    double throughputMbps = 0.0;
    double averageDelayMs = 0.0;
    double pdr = 0.0;
    double dropRatio = 0.0;
    double energyJ = 0.0;
    uint64_t txPackets = 0;
    uint64_t rxPackets = 0;
    uint64_t txBytes = 0;
    uint64_t rxBytes = 0;
};

class LrWpanEnergyTracker
{
  public:
    explicit LrWpanEnergyTracker(double supplyVoltage)
        : m_supplyVoltage(supplyVoltage)
    {
    }

    void
    Initialize(uint32_t nodeId, PhyEnumeration initialState)
    {
        m_entries[nodeId] = {Simulator::Now(), initialState, 0.0};
    }

    void
    OnStateChange(uint32_t nodeId, Time now, PhyEnumeration oldState, PhyEnumeration newState)
    {
        auto& entry = m_entries[nodeId];

        if (entry.lastChange == Time())
        {
            entry.lastChange = now;
            entry.currentState = oldState;
        }

        entry.energyJ += GetCurrentA(entry.currentState) * m_supplyVoltage *
                         (now - entry.lastChange).GetSeconds();
        entry.lastChange = now;
        entry.currentState = newState;
    }

    void
    Finalize(Time stopTime)
    {
        for (auto& [nodeId, entry] : m_entries)
        {
            (void) nodeId;
            entry.energyJ += GetCurrentA(entry.currentState) * m_supplyVoltage *
                             (stopTime - entry.lastChange).GetSeconds();
            entry.lastChange = stopTime;
        }
    }

    double
    GetTotalEnergyJ() const
    {
        double total = 0.0;
        for (const auto& [nodeId, entry] : m_entries)
        {
            (void) nodeId;
            total += entry.energyJ;
        }
        return total;
    }

  private:
    struct Entry
    {
        Time lastChange{Seconds(0)};
        PhyEnumeration currentState{IEEE_802_15_4_PHY_RX_ON};
        double energyJ{0.0};
    };

    static double
    GetCurrentA(PhyEnumeration state)
    {
        switch (state)
        {
        case IEEE_802_15_4_PHY_BUSY_TX:
        case IEEE_802_15_4_PHY_TX_ON:
            return 0.0174;
        case IEEE_802_15_4_PHY_BUSY_RX:
        case IEEE_802_15_4_PHY_RX_ON:
            return 0.0188;
        case IEEE_802_15_4_PHY_TRX_OFF:
            return 1e-6;
        default:
            return 0.000426;
        }
    }

    double m_supplyVoltage;
    std::map<uint32_t, Entry> m_entries;
};

void
TrackLrWpanState(LrWpanEnergyTracker* tracker,
                 uint32_t nodeId,
                 Time time,
                 PhyEnumeration oldState,
                 PhyEnumeration newState)
{
    tracker->OnStateChange(nodeId, time, oldState, newState);
}

void
ConfigureRtoMode(const std::string& mode)
{
    bool useAdaptive = false;
    bool useElRto = false;

    if (mode == "improved")
    {
        useAdaptive = true;
    }
    else if (mode == "elrto")
    {
        useAdaptive = true;
        useElRto = true;
    }
    else if (mode != "standard")
    {
        NS_FATAL_ERROR("Unsupported mode: " << mode << ". Use standard|improved|elrto");
    }

    Config::SetDefault("ns3::RttMeanDeviation::UseAdaptive", BooleanValue(useAdaptive));
    Config::SetDefault("ns3::RttMeanDeviation::UseElRto", BooleanValue(useElRto));
    Config::SetDefault("ns3::RttMeanDeviation::ElRtoWindow", UintegerValue(4));
    Config::SetDefault("ns3::RttMeanDeviation::ElRtoTheta", DoubleValue(2.0));
    Config::SetDefault("ns3::TcpSocketBase::MinRto", TimeValue(MilliSeconds(1)));
}

Ptr<ListPositionAllocator>
CreateServerCenteredAllocator(double areaSide, uint32_t stations)
{
    auto allocator = CreateObject<ListPositionAllocator>();
    const double center = areaSide / 2.0;
    allocator->Add(Vector(center, center, 0.0));

    Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable>();
    rng->SetAttribute("Min", DoubleValue(0.0));
    rng->SetAttribute("Max", DoubleValue(areaSide));

    for (uint32_t i = 0; i < stations; ++i)
    {
        allocator->Add(Vector(rng->GetValue(), rng->GetValue(), 0.0));
    }

    return allocator;
}

void
InstallMobileStations(NodeContainer stations, double areaSide, double speed)
{
    MobilityHelper mobility;
    auto waypointAllocator = CreateObjectWithAttributes<RandomRectanglePositionAllocator>(
        "X",
        StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaSide) + "]"),
        "Y",
        StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaSide) + "]"));

    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X",
                                  StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" +
                                              std::to_string(areaSide) + "]"),
                                  "Y",
                                  StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" +
                                              std::to_string(areaSide) + "]"));
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed",
                              StringValue("ns3::ConstantRandomVariable[Constant=" +
                                          std::to_string(speed) + "]"),
                              "Pause",
                              StringValue("ns3::ConstantRandomVariable[Constant=0.0]"),
                              "PositionAllocator",
                              PointerValue(waypointAllocator));
    mobility.Install(stations);
}

void
InstallStaticNode(Ptr<Node> node, double x, double y)
{
    MobilityHelper mobility;
    auto allocator = CreateObject<ListPositionAllocator>();
    allocator->Add(Vector(x, y, 0.0));
    mobility.SetPositionAllocator(allocator);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(NodeContainer(node));
}

Metrics
CollectIpv4Metrics(FlowMonitorHelper& flowmon,
                   Ptr<FlowMonitor> monitor,
                   uint16_t portBase,
                   uint32_t flowCount,
                   double activeSeconds)
{
    Metrics metrics;
    monitor->CheckForLostPackets();

    auto classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    for (const auto& [flowId, stats] : monitor->GetFlowStats())
    {
        const auto tuple = classifier->FindFlow(flowId);
        if (tuple.protocol != 6)
        {
            continue;
        }
        if (tuple.destinationPort < portBase || tuple.destinationPort >= portBase + flowCount)
        {
            continue;
        }

        metrics.txPackets += stats.txPackets;
        metrics.rxPackets += stats.rxPackets;
        metrics.txBytes += stats.txBytes;
        metrics.rxBytes += stats.rxBytes;
        metrics.averageDelayMs += stats.delaySum.GetSeconds() * 1000.0;
    }

    if (metrics.rxPackets > 0)
    {
        metrics.averageDelayMs /= static_cast<double>(metrics.rxPackets);
    }
    if (metrics.txPackets > 0)
    {
        metrics.pdr = static_cast<double>(metrics.rxPackets) / static_cast<double>(metrics.txPackets);
        metrics.dropRatio = static_cast<double>(metrics.txPackets - metrics.rxPackets) /
                            static_cast<double>(metrics.txPackets);
    }

    metrics.throughputMbps = (metrics.rxBytes * 8.0) / (activeSeconds * 1e6);
    return metrics;
}

Metrics
CollectIpv6Metrics(FlowMonitorHelper& flowmon,
                   Ptr<FlowMonitor> monitor,
                   uint16_t portBase,
                   uint32_t flowCount,
                   double activeSeconds)
{
    Metrics metrics;
    monitor->CheckForLostPackets();

    auto classifier = DynamicCast<Ipv6FlowClassifier>(flowmon.GetClassifier6());
    for (const auto& [flowId, stats] : monitor->GetFlowStats())
    {
        const auto tuple = classifier->FindFlow(flowId);
        if (tuple.protocol != 6)
        {
            continue;
        }
        if (tuple.destinationPort < portBase || tuple.destinationPort >= portBase + flowCount)
        {
            continue;
        }

        metrics.txPackets += stats.txPackets;
        metrics.rxPackets += stats.rxPackets;
        metrics.txBytes += stats.txBytes;
        metrics.rxBytes += stats.rxBytes;
        metrics.averageDelayMs += stats.delaySum.GetSeconds() * 1000.0;
    }

    if (metrics.rxPackets > 0)
    {
        metrics.averageDelayMs /= static_cast<double>(metrics.rxPackets);
    }
    if (metrics.txPackets > 0)
    {
        metrics.pdr = static_cast<double>(metrics.rxPackets) / static_cast<double>(metrics.txPackets);
        metrics.dropRatio = static_cast<double>(metrics.txPackets - metrics.rxPackets) /
                            static_cast<double>(metrics.txPackets);
    }

    metrics.throughputMbps = (metrics.rxBytes * 8.0) / (activeSeconds * 1e6);
    return metrics;
}

void
AppendResultRow(const std::string& outputCsv,
                const std::string& network,
                const std::string& mode,
                uint32_t nodes,
                uint32_t flows,
                uint32_t packetsPerSecond,
                double speed,
                double areaScale,
                uint32_t packetSize,
                const Metrics& metrics)
{
    const bool exists = std::ifstream(outputCsv).good();
    std::ofstream out(outputCsv, std::ios::app);

    if (!exists)
    {
        out << "network,mode,nodes,flows,pps,speed,areaScale,packetSize,throughputMbps,"
               "averageDelayMs,pdr,dropRatio,energyJ,txPackets,rxPackets,txBytes,rxBytes\n";
    }

    out << network << "," << mode << "," << nodes << "," << flows << "," << packetsPerSecond
        << "," << speed << "," << areaScale << "," << packetSize << ","
        << std::fixed << std::setprecision(6) << metrics.throughputMbps << ","
        << metrics.averageDelayMs << "," << metrics.pdr << "," << metrics.dropRatio << ","
        << metrics.energyJ << "," << metrics.txPackets << "," << metrics.rxPackets << ","
        << metrics.txBytes << "," << metrics.rxBytes << "\n";
}

Metrics
RunWifiMobile(uint32_t nodes,
              uint32_t flows,
              uint32_t packetSize,
              uint32_t packetsPerSecond,
              double speed,
              double areaScale,
              double simTime)
{
    const uint16_t portBase = 9000;
    const double baseArea = 40.0;
    const double areaSide = baseArea * areaScale;
    const double appStart = 2.0;
    const double activeSeconds = simTime - appStart - 1.0;

    NodeContainer allNodes;
    allNodes.Create(nodes);

    Ptr<Node> apNode = allNodes.Get(0);
    NodeContainer staNodes;
    for (uint32_t i = 1; i < nodes; ++i)
    {
        staNodes.Add(allNodes.Get(i));
    }

    InstallStaticNode(apNode, areaSide / 2.0, areaSide / 2.0);
    InstallMobileStations(staNodes, areaSide, speed);

    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode",
                       StringValue("DsssRate11Mbps"));
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue("DsssRate11Mbps"),
                                 "ControlMode",
                                 StringValue("DsssRate11Mbps"));

    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel",
                               "Frequency",
                               DoubleValue(2.4e9));

    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.SetErrorRateModel("ns3::YansErrorRateModel");

    Ssid ssid("rto-checklist");
    wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, wifiMac, apNode);

    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevices = wifi.Install(phy, wifiMac, staNodes);

    NetDeviceContainer allDevices;
    allDevices.Add(apDevice);
    allDevices.Add(staDevices);

    BasicEnergySourceHelper energySource;
    energySource.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(10000.0));
    auto sources = energySource.Install(allNodes);

    WifiRadioEnergyModelHelper radioEnergy;
    radioEnergy.Install(allDevices, sources);

    InternetStackHelper internet;
    internet.Install(allNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.10.0.0", "255.255.0.0");
    Ipv4InterfaceContainer apInterface = ipv4.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = ipv4.Assign(staDevices);
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    const Ipv4Address serverAddress = apInterface.GetAddress(0);

    ApplicationContainer sinks;
    ApplicationContainer clients;
    const uint64_t rateBps = static_cast<uint64_t>(packetSize) * packetsPerSecond * 8ull;

    for (uint32_t i = 0; i < flows; ++i)
    {
        const uint16_t port = portBase + i;
        PacketSinkHelper sink("ns3::TcpSocketFactory",
                              InetSocketAddress(Ipv4Address::GetAny(), port));
        sinks.Add(sink.Install(apNode));

        OnOffHelper client("ns3::TcpSocketFactory", Address());
        client.SetAttribute("Remote",
                            AddressValue(InetSocketAddress(serverAddress, port)));
        client.SetAttribute("OnTime",
                            StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        client.SetAttribute("OffTime",
                            StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));
        client.SetAttribute("DataRate", DataRateValue(DataRate(rateBps)));
        clients.Add(client.Install(staNodes.Get(i % staNodes.GetN())));
    }

    sinks.Start(Seconds(1.0));
    sinks.Stop(Seconds(simTime));
    clients.Start(Seconds(appStart));
    clients.Stop(Seconds(simTime - 1.0));

    FlowMonitorHelper flowmon;
    auto monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    Metrics metrics = CollectIpv4Metrics(flowmon, monitor, portBase, flows, activeSeconds);

    for (uint32_t i = 0; i < sources.GetN(); ++i)
    {
        auto source = DynamicCast<energy::BasicEnergySource>(sources.Get(i));
        if (!source)
        {
            continue;
        }
        metrics.energyJ += source->GetInitialEnergy() - source->GetRemainingEnergy();
    }

    Simulator::Destroy();
    return metrics;
}

Metrics
RunLrWpanMobile(uint32_t nodes,
                uint32_t flows,
                uint32_t packetSize,
                uint32_t packetsPerSecond,
                double speed,
                double areaScale,
                double simTime)
{
    const uint16_t portBase = 10000;
    const double baseArea = 40.0;
    const double areaSide = baseArea * areaScale;
    const double appStart = 2.0;
    const double activeSeconds = simTime - appStart - 1.0;

    NodeContainer allNodes;
    allNodes.Create(nodes);

    Ptr<Node> coordinator = allNodes.Get(0);
    NodeContainer mobileNodes;
    for (uint32_t i = 1; i < nodes; ++i)
    {
        mobileNodes.Add(allNodes.Get(i));
    }

    InstallStaticNode(coordinator, areaSide / 2.0, areaSide / 2.0);
    InstallMobileStations(mobileNodes, areaSide, speed);

    LrWpanHelper lrWpan;
    lrWpan.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    lrWpan.AddPropagationLossModel("ns3::LogDistancePropagationLossModel");
    NetDeviceContainer lrDevices = lrWpan.Install(allNodes);
    lrWpan.CreateAssociatedPan(lrDevices, 0x1234);

    LrWpanEnergyTracker energyTracker(3.0);
    for (uint32_t i = 0; i < lrDevices.GetN(); ++i)
    {
        auto dev = DynamicCast<LrWpanNetDevice>(lrDevices.Get(i));
        energyTracker.Initialize(i, IEEE_802_15_4_PHY_RX_ON);
        dev->GetPhy()->TraceConnectWithoutContext(
            "TrxState",
            MakeBoundCallback(&TrackLrWpanState, &energyTracker, i));
    }

    InternetStackHelper internet;
    internet.SetIpv4StackInstall(false);
    internet.Install(allNodes);

    SixLowPanHelper sixlowpan;
    NetDeviceContainer sixDevices = sixlowpan.Install(lrDevices);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64));
    auto interfaces = ipv6.Assign(sixDevices);
    const Ipv6Address serverAddress = interfaces.GetAddress(0, 1);

    ApplicationContainer sinks;
    ApplicationContainer clients;
    const uint64_t rateBps = static_cast<uint64_t>(packetSize) * packetsPerSecond * 8ull;

    for (uint32_t i = 0; i < flows; ++i)
    {
        const uint16_t port = portBase + i;
        PacketSinkHelper sink("ns3::TcpSocketFactory",
                              Inet6SocketAddress(Ipv6Address::GetAny(), port));
        sinks.Add(sink.Install(coordinator));

        OnOffHelper client("ns3::TcpSocketFactory", Address());
        client.SetAttribute("Remote",
                            AddressValue(Inet6SocketAddress(serverAddress, port)));
        client.SetAttribute("OnTime",
                            StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        client.SetAttribute("OffTime",
                            StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));
        client.SetAttribute("DataRate", DataRateValue(DataRate(rateBps)));
        clients.Add(client.Install(mobileNodes.Get(i % mobileNodes.GetN())));
    }

    sinks.Start(Seconds(1.0));
    sinks.Stop(Seconds(simTime));
    clients.Start(Seconds(appStart));
    clients.Stop(Seconds(simTime - 1.0));

    FlowMonitorHelper flowmon;
    auto monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    Metrics metrics = CollectIpv6Metrics(flowmon, monitor, portBase, flows, activeSeconds);
    energyTracker.Finalize(Seconds(simTime));
    metrics.energyJ = energyTracker.GetTotalEnergyJ();

    Simulator::Destroy();
    return metrics;
}

} // namespace

int
main(int argc, char* argv[])
{
    std::string network = "wifi-mobile";
    std::string mode = "standard";
    uint32_t nodes = 20;
    uint32_t flows = 10;
    uint32_t packetsPerSecond = 100;
    uint32_t packetSize = 0;
    double speed = 5.0;
    double areaScale = 2.0;
    double simTime = 20.0;
    std::string outputCsv = "checklist_wireless_results.csv";

    CommandLine cmd(__FILE__);
    cmd.AddValue("network", "wifi-mobile or lrwpan-mobile", network);
    cmd.AddValue("mode", "standard, improved, or elrto", mode);
    cmd.AddValue("nodes", "Total node count including the sink/coordinator", nodes);
    cmd.AddValue("flows", "Number of concurrent TCP flows", flows);
    cmd.AddValue("pps", "Packets per second per flow", packetsPerSecond);
    cmd.AddValue("packetSize", "TCP application packet size in bytes", packetSize);
    cmd.AddValue("speed", "Mobile node speed in meters per second", speed);
    cmd.AddValue("areaScale", "Area multiplier for node placement square", areaScale);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("outputCsv", "CSV file to append summary results to", outputCsv);
    cmd.Parse(argc, argv);

    if (nodes < 2)
    {
        NS_FATAL_ERROR("nodes must be at least 2");
    }

    const uint32_t maxUsefulFlows = nodes - 1;
    if (flows == 0 || flows > maxUsefulFlows)
    {
        flows = maxUsefulFlows;
    }

    if (packetSize == 0)
    {
        packetSize = (network == "lrwpan-mobile") ? 80 : 512;
    }

    ConfigureRtoMode(mode);

    Metrics metrics;
    if (network == "wifi-mobile")
    {
        metrics =
            RunWifiMobile(nodes, flows, packetSize, packetsPerSecond, speed, areaScale, simTime);
    }
    else if (network == "lrwpan-mobile")
    {
        metrics = RunLrWpanMobile(
            nodes, flows, packetSize, packetsPerSecond, speed, areaScale, simTime);
    }
    else
    {
        NS_FATAL_ERROR("Unsupported network: " << network);
    }

    AppendResultRow(
        outputCsv, network, mode, nodes, flows, packetsPerSecond, speed, areaScale, packetSize, metrics);

    std::cout << "network=" << network << "\n"
              << "mode=" << mode << "\n"
              << "throughputMbps=" << metrics.throughputMbps << "\n"
              << "averageDelayMs=" << metrics.averageDelayMs << "\n"
              << "pdr=" << metrics.pdr << "\n"
              << "dropRatio=" << metrics.dropRatio << "\n"
              << "energyJ=" << metrics.energyJ << "\n";

    return 0;
}
