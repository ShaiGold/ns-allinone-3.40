 /**- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
 /*
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
 * Author: George F. Riley<riley@ece.gatech.edu>
 */

#include <iostream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h" // TODO in this install stack quic
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/bulk-send-helper.h"
#include "ns3/tcp-bbr.h"
#include "ns3/flow-monitor-module.h" 
#include "ns3/quic-helper.h"
#include "ns3/quic-socket-factory.h"
#include "ns3/tcp-bbr.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/quic-module.h"
#include "ns3/quic-client-server-helper.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  std::string pacingRate = "10Mbps";

  //Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (512));
  //Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("500kb/s"));
  //Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpBbr"));
    
  // The maximum send buffer size is set to 4194304 bytes (4MB) and the
  // maximum receive buffer size is set to 6291456 bytes (6MB) in the Linux
  // kernel. The same buffer sizes are used as default in this example.
  /*Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(4194304));
  Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(6291456));
  Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(10));
  Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(2));
  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
  Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize", QueueSizeValue(QueueSize("1p")));
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpBbr"));*/
  Config::SetDefault ("ns3::TcpSocketState::MaxPacingRate", StringValue (pacingRate));
  Config::SetDefault ("ns3::TcpSocketState::EnablePacing", BooleanValue (true));
  Time stopTime = Seconds(100);
  uint32_t    nLeaf = 2; // If non-zero, number of both left and right

  uint32_t    nLeftLeaf = nLeaf;
  uint32_t    nRightLeaf = nLeaf;
  std::string animFile = "dumbbell-animation.xml" ;  // Name of file for animation output
  bool tracing = true;
  uint32_t maxBytes = 0;
  uint32_t QUICFlows = 1;
  bool isPacingEnabled = true;
  uint32_t maxPackets = 0;
  CommandLine cmd;
  cmd.AddValue ("nLeftLeaf", "Number of left side leaf nodes", nLeftLeaf);
  cmd.AddValue ("nRightLeaf","Number of right side leaf nodes", nRightLeaf);
  cmd.AddValue ("nLeaf",     "Number of left and right side leaf nodes", nLeaf);
  cmd.AddValue ("animFile",  "File Name for Animation Output", animFile);
  cmd.AddValue ("tracing", "Flag to enable/disable tracing", tracing);
  cmd.AddValue ("maxBytes",
                "Total number of bytes for application to send", maxBytes);
  cmd.AddValue ("maxPackets",
                "Total number of bytes for application to send", maxPackets);
  cmd.AddValue ("QUICFlows", "Number of application flows between sender and receiver", QUICFlows);
  cmd.AddValue ("Pacing", "Flag to enable/disable pacing in QUIC", isPacingEnabled);
  cmd.AddValue ("PacingRate", "Max Pacing Rate in bps", pacingRate);
  cmd.Parse (argc,argv);

  // Create the point-to-point link helpers
  PointToPointHelper pointToPointRouter;
  pointToPointRouter.SetDeviceAttribute  ("DataRate", StringValue ("10Mbps"));
  pointToPointRouter.SetChannelAttribute ("Delay", StringValue ("1ms"));
  PointToPointHelper pointToPointLeaf1;
  pointToPointLeaf1.SetDeviceAttribute    ("DataRate", StringValue ("10Mbps"));
  pointToPointLeaf1.SetChannelAttribute   ("Delay", StringValue ("5ms"));
  PointToPointHelper pointToPointLeaf2;
  pointToPointLeaf2.SetDeviceAttribute    ("DataRate", StringValue ("10Mbps"));
  pointToPointLeaf2.SetChannelAttribute   ("Delay", StringValue ("50ms"));
  PointToPointDumbbellHelper d (nLeftLeaf, 
                                pointToPointLeaf1,
                                pointToPointLeaf2,
                                nRightLeaf, pointToPointLeaf1,
                                pointToPointRouter);

  
  // Install Stack
  QuicHelper stack;
  //TODO d.InstallStackQuic(stack);

  // Assign IP Addresses
  d.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0", "255.255.255.0"),
                         Ipv4AddressHelper ("10.2.1.0", "255.255.255.0"),
                         Ipv4AddressHelper ("10.3.1.0", "255.255.255.0"));

  uint32_t numFlows = d.RightCount(); // Adjust as needed
  
  for (uint32_t i = 0; i < numFlows; ++i)
  {
// Select sender side port
    uint16_t port = 10000 + i;
    
    // Install application on the sender
    BulkSendHelper source("ns3::QuicSocketFactory", InetSocketAddress(d.GetLeftIpv4Address(i), port));
    source.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer sourceApps = source.Install(d.GetRight(i));
    sourceApps.Start(Seconds(0.00));
    // Hook trace source after application starts
    sourceApps.Stop(stopTime);
 
    // Install application on the receiver
    PacketSinkHelper sink("ns3::QuicSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(d.GetLeft(i));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(stopTime);
  }  
  // Set the bounding box for animation
  d.BoundingBox (1, 1, 100, 100);

  // Create the animation object and configure for specified output
  AnimationInterface anim (animFile);
  anim.EnablePacketMetadata (); // Optional
  anim.EnableIpv4L3ProtocolCounters (Seconds (0), Seconds (10)); // Optional
  
  // Set up the acutal simulation
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();

  Simulator::Stop(stopTime);

  Simulator::Run ();
  std::cout << "Animation Trace file created:" << animFile.c_str ()<< std::endl;
  flowMonitor->SerializeToXmlFile("flowmon.xml", true, true);

  Simulator::Destroy ();
  return 0;
}