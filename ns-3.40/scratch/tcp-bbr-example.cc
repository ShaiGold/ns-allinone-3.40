/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2018-20 NITK Surathkal
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Aarti Nandagiri <aarti.nandagiri@gmail.com>
 *          Vivek Jain <jain.vivek.anand@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

// This program simulates the following topology:
//
//           1000 Mbps           10Mbps          1000 Mbps
//  Sender -------------- R1 -------------- R2 -------------- Receiver
//              5ms               10ms               5ms
//
// Sender2 25 ms

// The link between R1 and R2 is a bottleneck link with 10 Mbps. All other
// links are 1000 Mbps.
//
// This program runs by default for 100 seconds and creates a new directory
// called 'bbr-results' in the ns-3 root directory. The program creates one
// sub-directory called 'pcap' in 'bbr-results' directory (if pcap generation
// is enabled) and three .dat files.
//
// (1) 'pcap' sub-directory contains six PCAP files:
//     * bbr-0-0.pcap for the interface on Sender
//     * bbr-1-0.pcap for the interface on Receiver
//     * bbr-2-0.pcap for the first interface on R1
//     * bbr-2-1.pcap for the second interface on R1
//     * bbr-3-0.pcap for the first interface on R2
//     * bbr-3-1.pcap for the second interface on R2
// (2) cwnd.dat file contains congestion window trace for the sender node
// (3) throughput.dat file contains sender side throughput trace
// (4) queueSize.dat file contains queue length trace from the bottleneck link
//
// BBR algorithm enters PROBE_RTT phase in every 10 seconds. The congestion
// window is fixed to 4 segments in this phase with a goal to achieve a better
// estimate of minimum RTT (because queue at the bottleneck link tends to drain
// when the congestion window is reduced to 4 segments).
//
// The congestion window and queue occupancy traces output by this program show
// periodic drops every 10 seconds when BBR algorithm is in PROBE_RTT phase.

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

std::string dir;
uint32_t prev = 0;
Time prevTime = Seconds (0);

// Calculate throughput
static void
TraceThroughput(Ptr<FlowMonitor> monitor)
{
  int connectionIndex = 0;
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  auto itr = stats.begin();
  Time curTime = Now();

  // Generate unique throughput file name based on connection index
  std::string fileName = dir + "/throughput" + std::to_string(connectionIndex) + ".dat";

  std::ofstream thr(fileName, std::ios::out | std::ios::app);

  // Convert time to seconds and use GetSeconds()
  thr << curTime.GetSeconds() << " " << 8 * (itr->second.txBytes - prev) / (1000 * 1000 * (curTime.GetSeconds() - prevTime.GetSeconds())) << std::endl;

  prevTime = curTime;
  prev = itr->second.txBytes;

  Simulator::Schedule(Seconds(0.2), &TraceThroughput, monitor, connectionIndex);
}

static void
TraceThroughput1(Ptr<FlowMonitor> monitor)
{
  int connectionIndex = 1;
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  auto itr = stats.begin();
  Time curTime = Now();

  // Generate unique throughput file name based on connection index
  std::string fileName = dir + "/throughput" + std::to_string(connectionIndex) + ".dat";

  std::ofstream thr(fileName, std::ios::out | std::ios::app);

  // Convert time to seconds and use GetSeconds()
  thr << curTime.GetSeconds() << " " << 8 * (itr->second.txBytes - prev) / (1000 * 1000 * (curTime.GetSeconds() - prevTime.GetSeconds())) << std::endl;

  prevTime = curTime;
  prev = itr->second.txBytes;

  Simulator::Schedule(Seconds(0.2), &TraceThroughput1, monitor, connectionIndex);
}

// Check the queue size
void CheckQueueSize (Ptr<QueueDisc> qd)
{
  uint32_t qsize = qd->GetCurrentSize ().GetValue ();
  Simulator::Schedule (Seconds (0.2), &CheckQueueSize, qd);
  std::ofstream q (dir + "/queueSize.dat", std::ios::out | std::ios::app);
  q << Simulator::Now ().GetSeconds () << " " << qsize << std::endl;
  q.close ();
}

// Trace congestion window
static void CwndTracer(Ptr<OutputStreamWrapper> stream, uint32_t oldval, uint32_t newval)
{
  *stream->GetStream() << Simulator::Now().GetSeconds() << " " << newval / 1448.0 << std::endl;
}

void TraceCwnd(uint32_t nodeId, uint32_t socketId, int connectionIndex)
{
  AsciiTraceHelper ascii;
  
  // Generate unique congestion window file name based on connection index
  std::string fileName = dir + "/cwnd" + std::to_string(connectionIndex) + ".dat";
  
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream(fileName);
  Config::ConnectWithoutContext("/NodeList/" + std::to_string(nodeId) + "/$ns3::TcpL4Protocol/SocketList/" + std::to_string(socketId) + "/CongestionWindow", MakeBoundCallback(&CwndTracer, stream));
}

int main (int argc, char *argv [])
{
    // Naming the output directory using local system time
    time_t rawtime;
    struct tm * timeinfo;
    char buffer [80];
    time (&rawtime);
    timeinfo = localtime (&rawtime);
    strftime (buffer, sizeof (buffer), "%d-%m-%Y-%I-%M-%S", timeinfo);
    std::string currentTime (buffer);

    std::string tcpTypeId = "TcpBbr";
    std::string queueDisc = "FifoQueueDisc";
    uint32_t delAckCount = 2;
    bool bql = true;
    bool enablePcap = false;
    Time stopTime = Seconds (100);

    CommandLine cmd (__FILE__);
    cmd.AddValue ("tcpTypeId", "Transport protocol to use: TcpNewReno, TcpBbr", tcpTypeId);
    cmd.AddValue ("delAckCount", "Delayed ACK count", delAckCount);
    cmd.AddValue ("enablePcap", "Enable/Disable pcap file generation", enablePcap);
    cmd.AddValue ("stopTime", "Stop time for applications / simulation time will be stopTime + 1", stopTime);
    cmd.Parse (argc, argv);

    queueDisc = std::string ("ns3::") + queueDisc;

    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::" + tcpTypeId));
    Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (4194304));
    Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (6291456));
    Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (10));
    Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (delAckCount));
    Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1448));
    Config::SetDefault ("ns3::DropTailQueue<Packet>::MaxSize", QueueSizeValue (QueueSize ("1p")));
    Config::SetDefault (queueDisc + "::MaxSize", QueueSizeValue (QueueSize ("100p")));

    NodeContainer senders, receivers;
    NodeContainer routers;
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

    PointToPointHelper edgeLink25ms;
    edgeLink25ms.SetDeviceAttribute("DataRate", StringValue("1000Mbps"));
    edgeLink25ms.SetChannelAttribute("Delay", StringValue("25ms"));

    // Create NetDevice containers
    NetDeviceContainer senderEdges[2];
    Ipv4InterfaceContainer ir1s[2];
    TrafficControlHelper tch;

    for (uint32_t connectionIndex = 0; connectionIndex < 2; ++connectionIndex) {
        if (connectionIndex == 0)
            senderEdges[connectionIndex] = edgeLink.Install(senders.Get(connectionIndex), routers.Get(0));
        else
            senderEdges[connectionIndex] = edgeLink25ms.Install(senders.Get(connectionIndex), routers.Get(0));

        NetDeviceContainer r1r2 = bottleneckLink.Install(routers.Get(0), routers.Get(1));
        NetDeviceContainer receiverEdge = edgeLink.Install(routers.Get(1), receivers.Get(connectionIndex));

        // Install Stack, Configure the root queue discipline, and Assign IP addresses
        InternetStackHelper internet;
        internet.Install(senders);//.Get(connectionIndex));
        internet.Install(receivers);//.Get(connectionIndex));
        internet.Install(routers);

        tch.SetRootQueueDisc(queueDisc);

        if (bql) {
            tch.SetQueueLimits("ns3::DynamicQueueLimits", "HoldTime", StringValue("1000ms"));
        }

        tch.Install(senderEdges[connectionIndex]);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase(std::string("10.") + std::to_string(connectionIndex)+std::string(".0.0"), "255.255.255.0");

        Ipv4InterfaceContainer i1i2 = ipv4.Assign(r1r2);

        ipv4.NewNetwork();
        Ipv4InterfaceContainer is1 = ipv4.Assign(senderEdges[connectionIndex]);

        ipv4.NewNetwork();
        ir1s[connectionIndex] = ipv4.Assign(receiverEdge);

        // Populate routing tables
        Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    }


    // Define the port outside the loop since it's constant for all connections
    uint16_t port = 50001;

    for (uint32_t connectionIndex = 0; connectionIndex < 2; ++connectionIndex) {
        // Rest of your code...

        // Install application on the sender
        BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(ir1s[connectionIndex].GetAddress(1), port));
        source.SetAttribute("MaxBytes", UintegerValue(0));
        ApplicationContainer sourceApps = source.Install(senders.Get(connectionIndex));
        sourceApps.Start(Seconds(0.0));
        Simulator::Schedule(Seconds(0.2), &TraceCwnd, 0, 0 ,connectionIndex);  // Pass connectionIndex as the last argument
        sourceApps.Stop(stopTime);

        // Install application on the receiver
        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApps = sink.Install(receivers.Get(connectionIndex));
        sinkApps.Start(Seconds(0.0));
        sinkApps.Stop(stopTime);
    }
    
    // Create a new directory to store the output of the program
    dir = "bbr-results/" + currentTime + "/";
    std::string dirToSave = "mkdir -p " + dir;
    system (dirToSave.c_str ());

    // The plotting scripts are provided in the following repository, if needed:
    // https://github.com/mohittahiliani/BBR-Validation/
    //
    // Download 'PlotScripts' directory (which is inside ns-3 scripts directory)
    // from the link given above and place it in the ns-3 root directory.
    // Uncomment the following three lines to generate plots for Congestion
    // Window, sender side throughput and queue occupancy on the bottleneck link.
    //
    system (("cp -R PlotScripts/gnuplotScriptCwnd " + dir).c_str ());
    system (("cp -R PlotScripts/gnuplotScriptThroughput " + dir).c_str ());
    system (("cp -R PlotScripts/gnuplotScriptQueueSize " + dir).c_str ());

    // Trace the queue occupancy on the second interface of R1
    tch.Uninstall (routers.Get (0)->GetDevice (1));
    QueueDiscContainer qd;
    qd = tch.Install (routers.Get (0)->GetDevice (1));
      Simulator::ScheduleNow (&CheckQueueSize, qd.Get (0));

    // Generate PCAP traces if it is enabled
    if (enablePcap)
      {
        system ((dirToSave + "/pcap/").c_str ());
        bottleneckLink.EnablePcapAll (dir + "/pcap/bbr", true);
      }
    
    // Check for dropped packets using Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
    Simulator::Schedule (Seconds (0 + 0.000001), &TraceThroughput, monitor);
    
    // Create FlowMonitorHelper for the second connection
    FlowMonitorHelper flowmon2;
    Ptr<FlowMonitor> monitor2 = flowmon2.InstallAll();

    // Schedule TraceThroughput for the second connection
    Simulator::Schedule(Seconds(0 + 0.000001), &TraceThroughput1, monitor2);


    Simulator::Stop (stopTime + TimeStep (1));
    Simulator::Run ();
    Simulator::Destroy ();
    
    return 0;
}   