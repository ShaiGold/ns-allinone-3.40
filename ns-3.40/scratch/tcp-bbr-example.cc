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
TraceThroughput (Ptr<FlowMonitor> monitor)
{
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  auto itr = stats.begin();
  Time curTime = Now();
  std::ofstream thr(dir + "/throughput.dat", std::ios::out | std::ios::app);

  // Convert time to seconds and use GetSeconds()
  thr << curTime.GetSeconds() << " " << 8 * (itr->second.txBytes - prev) / (1000 * 1000 * (curTime.GetSeconds() - prevTime.GetSeconds())) << std::endl;

  prevTime = curTime;
  prev = itr->second.txBytes;

  Simulator::Schedule(Seconds(0.2), &TraceThroughput, monitor);}
// Calculate throughput
static void
TraceThroughput2 (Ptr<FlowMonitor> monitor)
{
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  auto itr = stats.begin();
  Time curTime = Now();
  std::ofstream thr(dir + "/throughput2.dat", std::ios::out | std::ios::app);

  // Convert time to seconds and use GetSeconds()
  thr << curTime.GetSeconds() << " " << 8 * (itr->second.txBytes - prev) / (1000 * 1000 * (curTime.GetSeconds() - prevTime.GetSeconds())) << std::endl;

  prevTime = curTime;
  prev = itr->second.txBytes;

  Simulator::Schedule(Seconds(0.2), &TraceThroughput2, monitor);}

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
static void CwndTracer (Ptr<OutputStreamWrapper> stream, uint32_t oldval, uint32_t newval)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval / 1448.0 << std::endl;
}

void TraceCwnd (uint32_t nodeId, uint32_t socketId)
{
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream (dir + "/cwnd.dat");
  Config::ConnectWithoutContext ("/NodeList/" + std::to_string (nodeId) + "/$ns3::TcpL4Protocol/SocketList/" + std::to_string (socketId) + "/CongestionWindow", MakeBoundCallback (&CwndTracer, stream));
}

// Trace congestion window2
static void CwndTracer2 (Ptr<OutputStreamWrapper> stream, uint32_t oldval, uint32_t newval)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval / 1448.0 << std::endl;
}

void TraceCwnd2 (uint32_t nodeId, uint32_t socketId)
{
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream (dir + "/cwnd2.dat");
  Config::ConnectWithoutContext ("/NodeList/" + std::to_string (nodeId) + "/$ns3::TcpL4Protocol/SocketList/" + std::to_string (socketId) + "/CongestionWindow", MakeBoundCallback (&CwndTracer2, stream));
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

  NodeContainer sender, receiver;
  NodeContainer routers;
  sender.Create (1);
  receiver.Create (1);
  routers.Create (2);
  NodeContainer sender2, receiver2;
  sender2.Create (1);
  receiver2.Create (1);
  
  // Create the point-to-point link helpers
  PointToPointHelper bottleneckLink;
  bottleneckLink.SetDeviceAttribute  ("DataRate", StringValue ("10Mbps"));
  bottleneckLink.SetChannelAttribute ("Delay", StringValue ("10ms"));

  PointToPointHelper edgeLink;
  edgeLink.SetDeviceAttribute  ("DataRate", StringValue ("1000Mbps"));
  edgeLink.SetChannelAttribute ("Delay", StringValue ("5ms"));
  
  PointToPointHelper edgeLink25ms;
  edgeLink25ms.SetDeviceAttribute  ("DataRate", StringValue ("1000Mbps"));
  edgeLink25ms.SetChannelAttribute ("Delay", StringValue ("25ms"));
  
  PointToPointHelper bottleneckLink2;
  bottleneckLink.SetDeviceAttribute  ("DataRate", StringValue ("10Mbps"));
  bottleneckLink.SetChannelAttribute ("Delay", StringValue ("10ms"));

  PointToPointHelper edgeLink2;
  edgeLink.SetDeviceAttribute  ("DataRate", StringValue ("1000Mbps"));
  edgeLink.SetChannelAttribute ("Delay", StringValue ("5ms"));
  
  // Create NetDevice containers
  NetDeviceContainer senderEdge = edgeLink.Install (sender.Get (0), routers.Get (0));
  NetDeviceContainer r1r2 = bottleneckLink.Install (routers.Get (0), routers.Get (1));
  NetDeviceContainer receiverEdge = edgeLink.Install (routers.Get (1), receiver.Get (0));
  

  NetDeviceContainer senderEdge2 = edgeLink25ms.Install (sender2.Get (0), routers.Get (0));
  NetDeviceContainer r1r2_2 = bottleneckLink2.Install (routers.Get (0), routers.Get (1));
  NetDeviceContainer receiverEdge2 = edgeLink2.Install (routers.Get (1), receiver2.Get (0));
  // Install Stack
  InternetStackHelper internet;
  internet.Install (sender);
  internet.Install (receiver);
  internet.Install (routers);

  InternetStackHelper internet2;
  internet2.Install (sender2);
  internet2.Install (receiver2);
  internet.Install (routers);
  // Configure the root queue discipline
  TrafficControlHelper tch,tch2;
  tch.SetRootQueueDisc (queueDisc);
  tch2.SetRootQueueDisc (queueDisc);
  if (bql)
    {
      tch.SetQueueLimits ("ns3::DynamicQueueLimits", "HoldTime", StringValue ("1000ms"));
      tch2.SetQueueLimits ("ns3::DynamicQueueLimits", "HoldTime", StringValue ("1000ms"));
    }

  tch.Install (senderEdge);
  tch2.Install (senderEdge2);
  tch.Install (receiverEdge);
  tch2.Install (receiverEdge2);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "255.255.255.0");

  Ipv4InterfaceContainer i1i2 = ipv4.Assign (r1r2);

  ipv4.NewNetwork ();
  Ipv4InterfaceContainer is1 = ipv4.Assign (senderEdge);

  ipv4.NewNetwork ();
  Ipv4InterfaceContainer ir1 = ipv4.Assign (receiverEdge);
  

  Ipv4AddressHelper ipv4_2;
  ipv4_2.SetBase ("10.1.0.0", "255.255.255.0");

  Ipv4InterfaceContainer i1i2_2 = ipv4_2.Assign (r1r2_2);

  ipv4_2.NewNetwork ();
  Ipv4InterfaceContainer is1_2 = ipv4_2.Assign (senderEdge2);

  ipv4_2.NewNetwork ();
  Ipv4InterfaceContainer ir1_2 = ipv4_2.Assign (receiverEdge2);

  // Populate routing tables
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Select sender side port
  uint16_t port = 50001;

  // Install application on the sender
  BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (ir1.GetAddress (1), port));
  BulkSendHelper source2 ("ns3::TcpSocketFactory", InetSocketAddress (ir1_2.GetAddress (1), port));
  source.SetAttribute ("MaxBytes", UintegerValue (0));
  source2.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer sourceApps = source.Install (sender.Get (0));
  ApplicationContainer sourceApps2 = source2.Install (sender2.Get (0));
  sourceApps.Start (Seconds (0.0));
  sourceApps2.Start (Seconds (0.0));
  Simulator::Schedule (Seconds (0.2), &TraceCwnd, 0, 0);
  Simulator::Schedule (Seconds (0.2), &TraceCwnd2, 0, 0);
  sourceApps.Stop (stopTime);
  sourceApps2.Stop (stopTime);

  // Install application on the receiver
  PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sink2 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (receiver.Get (0));
  ApplicationContainer sinkApps2 = sink2.Install (receiver2.Get (0));
  sinkApps.Start (Seconds (0.0));
  sinkApps2.Start (Seconds (0.0));
  sinkApps.Stop (stopTime);
  sinkApps2.Stop (stopTime);

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

  FlowMonitorHelper flowmon2;
  Ptr<FlowMonitor> monitor2 = flowmon2.InstallAll ();
  Simulator::Schedule (Seconds (0 + 0.000001), &TraceThroughput2, monitor2);

  Simulator::Stop (stopTime + TimeStep (1));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
