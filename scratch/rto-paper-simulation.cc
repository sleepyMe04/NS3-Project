/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Simulation to replicate the paper:
 * "Improved RTO Algorithm for TCP Retransmission Timeout"
 * by Xiao Jianliang and Zhang Kun
 * 
 * This implements the topology from Figure 1 and scenarios from Figures 2-3
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <algorithm>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RtoImprovedSimulation");

// Global variables for data collection
std::ofstream rttFile;
std::ofstream rtoFile;
std::ofstream throughputFile;

// Trace callbacks
void RttTrace(std::string context, Time oldval, Time newval)
{
    (void)oldval;
    double simTime = Simulator::Now().GetSeconds();
    rttFile << simTime << "," << context << "," << newval.GetMilliSeconds() << std::endl;
}

void RtoTrace(std::string context, Time oldval, Time newval)
{
    (void)oldval;
    double simTime = Simulator::Now().GetSeconds();
    rtoFile << simTime << "," << context << "," << newval.GetMilliSeconds() << std::endl;
}

int main(int argc, char *argv[])
{
    // ═══════════════════════════════════════════════════════════════
    // CONFIGURATION PARAMETERS (matching the paper)
    // ═══════════════════════════════════════════════════════════════
    
    uint32_t nSenders = 9;           // 9 sender nodes
    uint32_t nReceivers = 9;         // 9 receiver nodes
    uint32_t maxConnections = 9;     // Maximum concurrent TCP connections
    
    std::string accessRate = "10Mbps";     // Access link bandwidth
    std::string accessDelay = "2.5ms";     // Access link delay
    std::string bottleneckRate = "20Mbps"; // Bottleneck bandwidth
    std::string bottleneckDelay = "5ms";   // Bottleneck delay
    
    uint32_t packetSize = 1000;      // Packet size in bytes
    uint32_t queueSize = 20;         // Router queue size in packets
    uint64_t maxBytes = 100000000;   // 100 MB per connection
    std::string cbrRate = "1Mbps";   // UDP CBR rate per flow (paper mentions FTP + CBR)
    bool enableCbr = true;           // Enable UDP CBR background traffic
    
    bool useAdaptive = true;         // Enable/disable improved algorithm
    std::string scenario = "increase"; // "increase" or "decrease"
    uint32_t baseConnections = 3;    // Connections active before abrupt switch
    double transitionTime = 12.0;    // Time of abrupt load change (seconds)
    
    double simTime = 30.0;           // Total simulation time (seconds)
    
    // Parse command line arguments
    CommandLine cmd;
    cmd.AddValue("nConnections", "Number of TCP connections", maxConnections);
    cmd.AddValue("useAdaptive", "Use adaptive RTO algorithm", useAdaptive);
    cmd.AddValue("scenario", "Scenario: increase or decrease", scenario);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("baseConnections",
                 "Connections kept active before/after abrupt transition",
                 baseConnections);
    cmd.AddValue("transitionTime", "Abrupt transition time in seconds", transitionTime);
    cmd.AddValue("cbrRate", "UDP CBR rate per flow", cbrRate);
    cmd.AddValue("enableCbr", "Enable UDP CBR traffic", enableCbr);
    cmd.Parse(argc, argv);

    baseConnections = std::min(baseConnections, maxConnections);
    transitionTime = std::max(1.1, std::min(transitionTime, simTime - 1.0));
    
    // ═══════════════════════════════════════════════════════════════
    // CONFIGURE TCP AND RTO
    // ═══════════════════════════════════════════════════════════════
    
    // Set TCP variant to NewReno (standard)
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", 
                       StringValue("ns3::TcpNewReno"));
    
    // Configure RTO estimator
    Config::SetDefault("ns3::RttMeanDeviation::UseAdaptive", 
                       BooleanValue(useAdaptive));
    Config::SetDefault("ns3::RttMeanDeviation::Alpha", 
                       DoubleValue(0.125));
    Config::SetDefault("ns3::RttMeanDeviation::Beta", 
                       DoubleValue(0.25));
    Config::SetDefault("ns3::TcpSocketBase::MinRto", TimeValue(MilliSeconds(1)));
    
    // TCP socket buffer sizes
    Config::SetDefault("ns3::TcpSocket::SegmentSize", 
                       UintegerValue(packetSize));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", 
                       UintegerValue(131072));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", 
                       UintegerValue(131072));
    
    // Enable logging
    LogComponentEnable("RtoImprovedSimulation", LOG_LEVEL_INFO);
    
    // Open output files
    std::string prefix = useAdaptive ? "improved" : "standard";
    rttFile.open(prefix + "_rtt_" + scenario + ".csv");
    rtoFile.open(prefix + "_rto_" + scenario + ".csv");
    throughputFile.open(prefix + "_throughput_" + scenario + ".csv");
    
    rttFile << "Time,Context,RTT(ms)" << std::endl;
    rtoFile << "Time,Context,RTO(ms)" << std::endl;
    
    NS_LOG_INFO("═══════════════════════════════════════════════════");
    NS_LOG_INFO("RTO Improved Algorithm Simulation");
    NS_LOG_INFO("Scenario: " << scenario);
    NS_LOG_INFO("Adaptive: " << (useAdaptive ? "ENABLED" : "DISABLED"));
    NS_LOG_INFO("Connections: " << maxConnections);
    NS_LOG_INFO("═══════════════════════════════════════════════════");
    
    // ═══════════════════════════════════════════════════════════════
    // CREATE NODES (Figure 1 topology)
    // ═══════════════════════════════════════════════════════════════
    
    NodeContainer senders;
    senders.Create(nSenders);
    
    NodeContainer routers;
    routers.Create(2);
    
    NodeContainer receivers;
    receivers.Create(nReceivers);
    
    NS_LOG_INFO("Created " << nSenders << " senders, 2 routers, " 
                << nReceivers << " receivers");
    
    // ═══════════════════════════════════════════════════════════════
    // CREATE LINKS
    // ═══════════════════════════════════════════════════════════════
    
    // Access links (sender to router)
    PointToPointHelper accessLink;
    accessLink.SetDeviceAttribute("DataRate", StringValue(accessRate));
    accessLink.SetChannelAttribute("Delay", StringValue(accessDelay));
    accessLink.SetQueue("ns3::DropTailQueue",
                        "MaxSize", StringValue(std::to_string(queueSize) + "p"));
    
    // Bottleneck link (router to router)
    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", StringValue(bottleneckRate));
    bottleneckLink.SetChannelAttribute("Delay", StringValue(bottleneckDelay));
    bottleneckLink.SetQueue("ns3::DropTailQueue",
                            "MaxSize", StringValue(std::to_string(queueSize) + "p"));
    
    // Install access links: senders -> router[0]
    std::vector<NetDeviceContainer> senderDevices(nSenders);
    for (uint32_t i = 0; i < nSenders; i++)
    {
        NodeContainer pair(senders.Get(i), routers.Get(0));
        senderDevices[i] = accessLink.Install(pair);
    }
    
    // Install bottleneck link: router[0] <-> router[1]
    NetDeviceContainer bottleneckDevices = bottleneckLink.Install(routers);
    
    // Install access links: router[1] -> receivers
    std::vector<NetDeviceContainer> receiverDevices(nReceivers);
    for (uint32_t i = 0; i < nReceivers; i++)
    {
        NodeContainer pair(routers.Get(1), receivers.Get(i));
        receiverDevices[i] = accessLink.Install(pair);
    }
    
    NS_LOG_INFO("Links created successfully");
    
    // ═══════════════════════════════════════════════════════════════
    // INSTALL INTERNET STACK
    // ═══════════════════════════════════════════════════════════════
    
    InternetStackHelper stack;
    stack.Install(senders);
    stack.Install(routers);
    stack.Install(receivers);
    
    NS_LOG_INFO("Internet stack installed");
    
    // ═══════════════════════════════════════════════════════════════
    // ASSIGN IP ADDRESSES
    // ═══════════════════════════════════════════════════════════════
    
    Ipv4AddressHelper address;
    
    // Sender links
    for (uint32_t i = 0; i < nSenders; i++)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i+1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        address.Assign(senderDevices[i]);
    }
    
    // Bottleneck link
    address.SetBase("10.2.1.0", "255.255.255.0");
    Ipv4InterfaceContainer bottleneckInterfaces = address.Assign(bottleneckDevices);
    
    // Receiver links
    std::vector<Ipv4InterfaceContainer> receiverInterfaces(nReceivers);
    for (uint32_t i = 0; i < nReceivers; i++)
    {
        std::ostringstream subnet;
        subnet << "10.3." << i+1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        receiverInterfaces[i] = address.Assign(receiverDevices[i]);
    }
    
    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    
    NS_LOG_INFO("IP addresses assigned and routing configured");
    
    // ═══════════════════════════════════════════════════════════════
    // CREATE APPLICATIONS
    // ═══════════════════════════════════════════════════════════════
    
    uint16_t port = 9;
    uint16_t cbrPortBase = 10000;
    ApplicationContainer sinkApps;
    ApplicationContainer sourceApps;
    ApplicationContainer cbrSinkApps;
    ApplicationContainer cbrSourceApps;
    
    // Setup based on scenario
    if (scenario == "increase")
    {
        // ───────────────────────────────────────────────────────────
        // SCENARIO 1: RTT RAPID INCREASE (Figure 2)
        // Start with few connections, then abruptly add the rest
        // ───────────────────────────────────────────────────────────
        
        NS_LOG_INFO("Setting up RTT INCREASE scenario...");
        
        for (uint32_t i = 0; i < maxConnections; i++)
        {
            // Sink (receiver)
            Address sinkAddress(InetSocketAddress(
                receiverInterfaces[i].GetAddress(1), port));
            PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
            ApplicationContainer sink = sinkHelper.Install(receivers.Get(i));
            sink.Start(Seconds(0.0));
            sink.Stop(Seconds(simTime));
            sinkApps.Add(sink);
            
            // FTP-like TCP source (BulkSend)
            BulkSendHelper sourceHelper("ns3::TcpSocketFactory", sinkAddress);
            sourceHelper.SetAttribute("MaxBytes", UintegerValue(maxBytes));
            sourceHelper.SetAttribute("SendSize", UintegerValue(packetSize));
            ApplicationContainer source = sourceHelper.Install(senders.Get(i));
            
            // Abrupt step: a base set starts early, remaining flows start together.
            double startTime = (i < baseConnections) ? 1.0 : transitionTime;
            source.Start(Seconds(startTime));
            source.Stop(Seconds(simTime - 1.0));
            sourceApps.Add(source);

            // CBR stream (UDP) to emulate FTP + CBR traffic mix from paper text
            if (enableCbr)
            {
                uint16_t cbrPort = cbrPortBase + i;
                PacketSinkHelper cbrSinkHelper(
                    "ns3::UdpSocketFactory",
                    InetSocketAddress(Ipv4Address::GetAny(), cbrPort));
                ApplicationContainer cbrSink = cbrSinkHelper.Install(receivers.Get(i));
                cbrSink.Start(Seconds(0.0));
                cbrSink.Stop(Seconds(simTime));
                cbrSinkApps.Add(cbrSink);

                OnOffHelper cbrSourceHelper(
                    "ns3::UdpSocketFactory",
                    InetSocketAddress(receiverInterfaces[i].GetAddress(1), cbrPort));
                cbrSourceHelper.SetAttribute("DataRate", StringValue(cbrRate));
                cbrSourceHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
                cbrSourceHelper.SetAttribute("OnTime",
                                             StringValue("ns3::ConstantRandomVariable[Constant=1]"));
                cbrSourceHelper.SetAttribute("OffTime",
                                             StringValue("ns3::ConstantRandomVariable[Constant=0]"));
                cbrSourceHelper.SetAttribute("MaxBytes", UintegerValue(maxBytes));
                ApplicationContainer cbrSource = cbrSourceHelper.Install(senders.Get(i));
                cbrSource.Start(Seconds(startTime));
                cbrSource.Stop(Seconds(simTime - 1.0));
                cbrSourceApps.Add(cbrSource);
            }
            
            NS_LOG_INFO("Connection " << i << " starts at " << startTime << "s");
        }
    }
    else if (scenario == "decrease")
    {
        // ───────────────────────────────────────────────────────────
        // SCENARIO 2: RTT RAPID DECREASE (Figure 3)
        // Start with all connections, then abruptly remove most of them
        // ───────────────────────────────────────────────────────────
        
        NS_LOG_INFO("Setting up RTT DECREASE scenario...");
        
        for (uint32_t i = 0; i < maxConnections; i++)
        {
            // Sink (receiver)
            Address sinkAddress(InetSocketAddress(
                receiverInterfaces[i].GetAddress(1), port));
            PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
            ApplicationContainer sink = sinkHelper.Install(receivers.Get(i));
            sink.Start(Seconds(0.0));
            sink.Stop(Seconds(simTime));
            sinkApps.Add(sink);
            
            // FTP-like TCP source (BulkSend)
            BulkSendHelper sourceHelper("ns3::TcpSocketFactory", sinkAddress);
            sourceHelper.SetAttribute("MaxBytes", UintegerValue(maxBytes));
            sourceHelper.SetAttribute("SendSize", UintegerValue(packetSize));
            ApplicationContainer source = sourceHelper.Install(senders.Get(i));
            
            // All start together at 1s
            source.Start(Seconds(1.0));
            
            // Abrupt step: only baseConnections survive after transitionTime.
            double stopTime = (i < baseConnections) ? (simTime - 1.0) : transitionTime;
            source.Stop(Seconds(stopTime));
            sourceApps.Add(source);

            if (enableCbr)
            {
                uint16_t cbrPort = cbrPortBase + i;
                PacketSinkHelper cbrSinkHelper(
                    "ns3::UdpSocketFactory",
                    InetSocketAddress(Ipv4Address::GetAny(), cbrPort));
                ApplicationContainer cbrSink = cbrSinkHelper.Install(receivers.Get(i));
                cbrSink.Start(Seconds(0.0));
                cbrSink.Stop(Seconds(simTime));
                cbrSinkApps.Add(cbrSink);

                OnOffHelper cbrSourceHelper(
                    "ns3::UdpSocketFactory",
                    InetSocketAddress(receiverInterfaces[i].GetAddress(1), cbrPort));
                cbrSourceHelper.SetAttribute("DataRate", StringValue(cbrRate));
                cbrSourceHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
                cbrSourceHelper.SetAttribute("OnTime",
                                             StringValue("ns3::ConstantRandomVariable[Constant=1]"));
                cbrSourceHelper.SetAttribute("OffTime",
                                             StringValue("ns3::ConstantRandomVariable[Constant=0]"));
                cbrSourceHelper.SetAttribute("MaxBytes", UintegerValue(maxBytes));
                ApplicationContainer cbrSource = cbrSourceHelper.Install(senders.Get(i));
                cbrSource.Start(Seconds(1.0));
                cbrSource.Stop(Seconds(stopTime));
                cbrSourceApps.Add(cbrSource);
            }
            
            NS_LOG_INFO("Connection " << i << " stops at " << stopTime << "s");
        }
    }
    
    NS_LOG_INFO("Applications configured");
    
    // ═══════════════════════════════════════════════════════════════
    // CONNECT TRACE SOURCES (to capture RTT and RTO)
    // ═══════════════════════════════════════════════════════════════
    
    // Trace all TCP sockets and keep context in CSV for filtering.
    Simulator::Schedule(Seconds(2.0), []() {
        Config::Connect(
            "/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/RTT",
            MakeCallback(&RttTrace));
        Config::Connect(
            "/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/RTO",
            MakeCallback(&RtoTrace));
        NS_LOG_INFO("RTT and RTO tracing enabled (all sockets)");
    });
    
    // ═══════════════════════════════════════════════════════════════
    // RUN SIMULATION
    // ═══════════════════════════════════════════════════════════════
    
    NS_LOG_INFO("Starting simulation for " << simTime << " seconds...");
    
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    
    // ═══════════════════════════════════════════════════════════════
    // CALCULATE STATISTICS
    // ═══════════════════════════════════════════════════════════════
    
    NS_LOG_INFO("Simulation finished");
    
    // Calculate total bytes received
    uint64_t totalRxBytes = 0;
    for (uint32_t i = 0; i < sinkApps.GetN(); i++)
    {
        Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApps.Get(i));
        totalRxBytes += sink->GetTotalRx();
    }
    
    double throughput = (totalRxBytes * 8.0) / (simTime * 1000000.0); // Mbps
    
    NS_LOG_INFO("═══════════════════════════════════════════════════");
    NS_LOG_INFO("RESULTS:");
    NS_LOG_INFO("Total bytes received: " << totalRxBytes);
    NS_LOG_INFO("Average throughput: " << throughput << " Mbps");
    NS_LOG_INFO("═══════════════════════════════════════════════════");
    
    throughputFile << "Scenario,Algorithm,Throughput(Mbps)" << std::endl;
    throughputFile << scenario << "," << (useAdaptive ? "Improved" : "Standard") 
                   << "," << throughput << std::endl;
    
    // Close files
    rttFile.close();
    rtoFile.close();
    throughputFile.close();
    
    Simulator::Destroy();
    
    NS_LOG_INFO("Output files saved:");
    NS_LOG_INFO("  - " << prefix << "_rtt_" << scenario << ".csv");
    NS_LOG_INFO("  - " << prefix << "_rto_" << scenario << ".csv");
    NS_LOG_INFO("  - " << prefix << "_throughput_" << scenario << ".csv");
    
    return 0;
}
/*



./ns3 run "rto-smooth-simulation --useAdaptive=false --scenario=increase --simTime=30"
./ns3 run "rto-smooth-simulation --useAdaptive=true --scenario=increase --simTime=30"
./ns3 run "rto-smooth-simulation --useAdaptive=false --scenario=decrease --simTime=30"
./ns3 run "rto-smooth-simulation --useAdaptive=true --scenario=decrease --simTime=30"

python3 plot_results.py


*/
