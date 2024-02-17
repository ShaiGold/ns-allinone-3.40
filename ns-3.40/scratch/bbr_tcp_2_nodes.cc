#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/tcp-bbr.h"

#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include <iostream>


using namespace ns3;
using namespace ns3::SystemPath;

int main (int argc, char * argv[]){

    NodeContainer senders;
    NodeContainer receivers;
    NodeContainer routers;
    CommandLine cmd;

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpBbr"));
    
    // The maximum send buffer size is set to 4194304 bytes (4MB) and the
    // maximum receive buffer size is set to 6291456 bytes (6MB) in the Linux
    // kernel. The same buffer sizes are used as default in this example.
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(4194304));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(6291456));
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(10));
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(2));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize", QueueSizeValue(QueueSize("1p")));

    senders.Create(2);
    receivers.Create(2);
    routers.Create(2);

    // Create the point-to-point link helpers
    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    bottleneckLink.SetChannelAttribute("Delay", StringValue("10ms"));
 
    PointToPointHelper edgeLink;
    edgeLink.SetDeviceAttribute("DataRate", StringValue("1000Mbps"));
    edgeLink.SetChannelAttribute("Delay", StringValue("5ms"));
 
    PointToPointHelper edgeLink2;
    edgeLink2.SetDeviceAttribute("DataRate", StringValue("1000Mbps"));
    edgeLink2.SetChannelAttribute("Delay", StringValue("200ms"));

    NetDeviceContainer senderEdge1, senderEdge2, r1r2, receiverEdge1, receiverEdge2;
    senderEdge1 = edgeLink.Install(senders.Get(0), routers.Get(0));
    senderEdge2 = edgeLink2.Install(senders.Get(1), routers.Get(0));
    r1r2 = bottleneckLink.Install(routers.Get(0), routers.Get(1));
    receiverEdge1 = edgeLink.Install(routers.Get(1), receivers.Get(0));
    receiverEdge2 = edgeLink.Install(routers.Get(1), receivers.Get(1));
    
    AsciiTraceHelper ascii;
    bottleneckLink.EnableAsciiAll(ascii.CreateFileStream("ns3-traces.tr"));

    //install internet stack
    InternetStackHelper internet;
    internet.Install(senders);
    internet.Install(receivers);
    internet.Install(routers);
    
    //assign ip addresses
    Ipv4AddressHelper ipv4;
    ipv4.NewNetwork();
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer is1 = ipv4.Assign(senderEdge1);
    Ipv4InterfaceContainer is2 = ipv4.Assign(senderEdge2);
    
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer il = ipv4.Assign(r1r2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer ir2 = ipv4.Assign(receiverEdge2);

    Ipv4InterfaceContainer ir1 = ipv4.Assign(receiverEdge1);

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    // Set tcp connection
    uint16_t port1 = 8;
    uint16_t port2 = 9;
    

    // Set up Source app
    // Static routes for Router 1
    /*
    Ptr<Ipv4StaticRouting> staticRoutingR1 = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(routers.Get(0)->GetObject<Ipv4>());
    staticRoutingR1->AddHostRouteTo(Ipv4Address("10.1.3.1"), Ipv4Address("10.1.1.1"), 1); // Route to Receiver 1
    staticRoutingR1->AddHostRouteTo(Ipv4Address("10.1.3.2"), Ipv4Address("10.1.2.1"), 0); // Route to Receiver 2

    // Static routes for Router 2
    Ptr<Ipv4StaticRouting> staticRoutingR2 = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(routers.Get(1)->GetObject<Ipv4>());
    staticRoutingR2->AddHostRouteTo(Ipv4Address("10.1.1.1"), Ipv4Address("10.1.2.2"), 1); // Route to Sender 1
    staticRoutingR2->AddHostRouteTo(Ipv4Address("10.1.2.2"), Ipv4Address("10.1.3.1"), 0); // Route to Sender 2*/

    Address sinkAddress1(InetSocketAddress(Ipv4Address::GetAny(),port1));
    BulkSendHelper source1("ns3::TcpSocketFactory", InetSocketAddress(ir1.GetAddress(1), port1));
    source1.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer sourceApps1 = source1.Install(senders.Get(0));
    sourceApps1.Start(Seconds(0.1));
    sourceApps1.Stop(Seconds(10.0));

    Address sinkAddress2(InetSocketAddress(Ipv4Address::GetAny(),port2));
    BulkSendHelper source2("ns3::TcpSocketFactory", InetSocketAddress(ir2.GetAddress(1), port2));
    source2.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer sourceApps2 = source2.Install(senders.Get(1));
    sourceApps2.Start(Seconds(0.1));
    sourceApps2.Stop(Seconds(10.0));

    // Set up Server app
    PacketSinkHelper sink1("ns3::TcpSocketFactory", sinkAddress1);
    ApplicationContainer sinkApps1 = sink1.Install(receivers.Get(0));
    sinkApps1.Start(Seconds(0.0));
    sinkApps1.Stop(Seconds(10.0));

    PacketSinkHelper sink2("ns3::TcpSocketFactory", sinkAddress2);
    ApplicationContainer sinkApps2 = sink2.Install(receivers.Get(1));
    sinkApps2.Start(Seconds(0.0));
    sinkApps2.Stop(Seconds(10.0));

    //bottleneckLink.EnablePcapAll("edgeLink");

    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    Simulator::Stop(Seconds(10.0));
    std::string animFile = "dumbbell-animation.xml" ;  // Name of file for animation output
    cmd.AddValue ("animFile",  "File Name for Animation Output", animFile);
    AnimationInterface anim (animFile);
    
    //anim.SetConstantPosition (nodes.Get(0), 0, 5);
    //anim.SetConstantPosition (nodes.Get(1), 10, 5);
    Simulator::Schedule (Seconds (1), &Ipv4GlobalRoutingHelper::RecomputeRoutingTables);
    
    //Ipv4GlobalRoutingHelper::RecomputeRoutingTables();

    Simulator::Run();

    monitor->SerializeToXmlFile("name.xml", true, true);

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (auto it = stats.begin(); it != stats.end(); it++){
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        std::cout << "Flow " << t.sourceAddress << " -> " << t.destinationAddress 
        << " Bytes: " << it->second.txBytes 
        << " rBytes: " << it->second.rxBytes 
        << " Throughput: " << it->second.txBytes * 8.0 / (it->second.timeLastTxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds())
        << std::endl;
    }

    Simulator::Destroy();
    return 0;

}