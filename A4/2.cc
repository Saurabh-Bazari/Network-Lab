#include <bits/stdc++.h>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/tcp-westwood.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/tcp-hybla.h"
#include "ns3/tcp-congestion-ops.h"
#include "ns3/traced-value.h"
#include "ns3/tcp-yeah.h"
#include "ns3/log.h"
#include "ns3/tcp-scalable.h"
#include "ns3/sequence-number.h"
#include "ns3/traced-value.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/enum.h"

using namespace ns3;

double run_duration;
uint maxPacketSize, portNumber, totalPackets;
std::string dataTransferRate;

std::map<std::string, uint> mapPacketloss;
std::map<Address, double> dataAtDest;
std::map<std::string, double> ipv4ReceivedData, maxthroughPut;
NS_LOG_COMPONENT_DEFINE ("MyApp");

//------------------------------------------------------------------------------------------------------------------------------------
class MyApp : public Application
{
public:

  MyApp ();
  virtual ~MyApp();

  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
};

MyApp::MyApp ()
  : m_socket (0),
    m_peer (),
    m_packetSize (0),
    m_nPackets (0),
    m_dataRate (0),
    m_sendEvent (),
    m_running (false),
    m_packetsSent (0)
{
}

MyApp::~MyApp()
{
  m_socket = 0;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
}

void
MyApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  SendPacket ();
}

void
MyApp::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
MyApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
    }
}

void
MyApp::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

Ptr<Socket> FlowX(Address sinkAddress, Ptr<Node> sourceNode, Ptr<Node> destNode, std::string FlowType,double time_of_start)
{
  if(FlowType=="westwood"){
    Config::SetDefault ("ns3::TcpWestwood::ProtocolType", EnumValue (TcpWestwood::WESTWOODPLUS));
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpWestwood::GetTypeId()));
  }
  else if(FlowType=="hybla"){
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpHybla::GetTypeId()));
  }
  else{
    time_of_start=0;
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpYeah::GetTypeId()));
  }

	PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), portNumber));
	ApplicationContainer sinkContainer = packetSinkHelper.Install(destNode);
	sinkContainer.Start(Seconds(time_of_start));
	sinkContainer.Stop(Seconds(time_of_start + run_duration));

	Ptr<Socket> TcpFlowSocket = Socket::CreateSocket(sourceNode, TcpSocketFactory::GetTypeId());
	Ptr<MyApp> app = CreateObject<MyApp>();

	app->Setup(TcpFlowSocket, sinkAddress, maxPacketSize, totalPackets, DataRate(dataTransferRate));
	sourceNode->AddApplication(app);
	app->SetStartTime(Seconds(time_of_start));
	app->SetStopTime(Seconds(time_of_start + run_duration));

	return TcpFlowSocket;
}

void packetDropFunc(std::string flowType) {
	mapPacketloss[flowType]++;
}

void cwndFunc(FILE *fp, double time_of_start, uint prev_congestion_window, uint new_congestion_window) {
  fprintf(fp, "%s %s\n",(std::to_string( Simulator::Now ().GetSeconds () - time_of_start )).c_str()   , (std::to_string(new_congestion_window )).c_str() );

}

void goodputFunc(FILE *fp, double time_of_start, std::string type, Ptr<const Packet> pckt, const Address& address){
	dataAtDest[address] += pckt->GetSize();
  double newSpeed = (((dataAtDest[address] * 8.0) / 1024)/(Simulator::Now().GetSeconds()-time_of_start));
  fprintf(fp, "%s %s\n",(std::to_string( Simulator::Now().GetSeconds()-time_of_start).c_str() ) , (std::to_string(newSpeed)).c_str() );
}

void throughputFunc(FILE *fp, double time_of_start, std::string type, Ptr<const Packet> pckt, Ptr<Ipv4> ipv4, uint interface) {

  if(maxthroughPut.find(type) == maxthroughPut.end())
    maxthroughPut[type] = 0;

	ipv4ReceivedData[type] += pckt->GetSize();
	double newSpeed = (((ipv4ReceivedData[type] * 8.0) / 1024)/(Simulator::Now().GetSeconds()-time_of_start));
    fprintf(fp, "%s %s\n",(std::to_string( Simulator::Now().GetSeconds()-time_of_start).c_str())  , (std::to_string(newSpeed )).c_str() );

  if(maxthroughPut[type] < newSpeed)
    maxthroughPut[type] = newSpeed;
}

int main(){

  portNumber = 5000;
  double time_of_start = 0;
  run_duration = 1000;
  totalPackets = 1000000;
  maxPacketSize = 1.3*1024;
  dataTransferRate = "40Mbps";

  // Time::SetResolution (Time::NS);
  Ptr<RateErrorModel> errorM = CreateObjectWithAttributes<RateErrorModel> ("ErrorRate", DoubleValue (0.00001));

  PointToPointHelper rtor, htor;

  htor.SetChannelAttribute("Delay", StringValue("20ms"));
  rtor.SetChannelAttribute("Delay", StringValue("50ms"));

  htor.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  rtor.SetDeviceAttribute("DataRate", StringValue("10Mbps"));

  htor.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue((100000*20)/maxPacketSize));
  rtor.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue((10000*50)/maxPacketSize));

  NodeContainer node;
  node.Create(8);

  InternetStackHelper stackInternet;
  stackInternet.Install(node);

  Ipv4AddressHelper ipv4ah;

  NodeContainer yeah0r1 = NodeContainer(node.Get(0), node.Get(3));
  NetDeviceContainer ndc_yeah0r1 = htor.Install(yeah0r1);
  ndc_yeah0r1.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(errorM));
  ipv4ah.SetBase ("172.16.111.0", "255.255.255.0");
  Ipv4InterfaceContainer ip_yeah0r1 = ipv4ah.Assign(ndc_yeah0r1);

  NodeContainer hybla1r1 = NodeContainer(node.Get(1), node.Get(3));
  NetDeviceContainer ndc_hybla1r1 = htor.Install(hybla1r1);
  ndc_hybla1r1.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(errorM));
  ipv4ah.SetBase ("172.16.112.0", "255.255.255.0");
  Ipv4InterfaceContainer ip_hybla1r1 = ipv4ah.Assign(ndc_hybla1r1);

  NodeContainer westwood2r1 = NodeContainer(node.Get(2), node.Get(3));
  NetDeviceContainer ndc_westwood2r1 = htor.Install(westwood2r1);
  ndc_westwood2r1.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(errorM));
  ipv4ah.SetBase ("172.16.113.0", "255.255.255.0");
  Ipv4InterfaceContainer ip_westwood2r1 = ipv4ah.Assign(ndc_westwood2r1);

  NodeContainer yeah5r2 = NodeContainer(node.Get(5), node.Get(4));
  NetDeviceContainer ndc_yeah5r2 = htor.Install(yeah5r2);
  ndc_yeah5r2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(errorM));
  ipv4ah.SetBase ("172.16.115.0", "255.255.255.0");
  Ipv4InterfaceContainer ip_yeah5r2 = ipv4ah.Assign(ndc_yeah5r2);

  NodeContainer hybla6r2 = NodeContainer(node.Get(6), node.Get(4));
  NetDeviceContainer ndc_hybla6r2 = htor.Install(hybla6r2);
  ndc_hybla6r2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(errorM));
  ipv4ah.SetBase ("172.16.116.0", "255.255.255.0");
  Ipv4InterfaceContainer ip_hybla6r2 = ipv4ah.Assign(ndc_hybla6r2);

  NodeContainer westwood7r2 = NodeContainer(node.Get(7), node.Get(4));
  NetDeviceContainer ndc_westwood7r2 = htor.Install(westwood7r2);
  ndc_westwood7r2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(errorM));
  ipv4ah.SetBase ("172.16.117.0", "255.255.255.0");
  Ipv4InterfaceContainer ip_westwood7r2 = ipv4ah.Assign(ndc_westwood7r2);

  NodeContainer r3r4 = NodeContainer(node.Get(3), node.Get(4));
  NetDeviceContainer ndc_r3r4 = rtor.Install(r3r4);
  ipv4ah.SetBase ("172.16.114.0", "255.255.255.0");
  Ipv4InterfaceContainer ip_r3r4 = ipv4ah.Assign(ndc_r3r4);

  //FILE *yeahPacketDropFP = fopen("2_yeah_packetloss.txt", w);
  FILE *yeahCongestionWindowFP = fopen("2_yeah_congestionwindow.txt", "w");
  FILE *yeahThroughPutFP = fopen("2_yeah_throughput.txt", "w");
  FILE *yeahGoodPutFP = fopen("2_yeah_goodput.txt", "w");

  //FILE *hyblaPacketDropFP = fopen("2_hybla_packetloss.txt", "w");
  FILE *hyblaCongestionWindowFP = fopen("2_hybla_congestionwindow.txt", "w");
  FILE *hyblaThroughPutFP = fopen("2_hybla_throughput.txt", "w");
  FILE *hyblaGoodPutFP = fopen("2_hybla_goodput.txt", "w");

  //FILE *westwoodPacketDropFP = fopen("2_westwood_packetloss.txt", "w");
  FILE *westwoodCongestionWindowFP = fopen("2_westwood_congestionwindow.txt", "w");
  FILE *westwoodThroughPutFP = fopen("2_westwood_throughput.txt", "w");
  FILE *westwoodGoodPutFP = fopen("2_westwood_goodput.txt", "w");



  //-----------------------------------------------------------------------------------------------------
  Ptr<Socket> yeahSocket = FlowX(InetSocketAddress( ip_yeah5r2.GetAddress(0), portNumber), node.Get(0), node.Get(5), "yeah",time_of_start);

	Config::Connect("/NodeList/5/ApplicationList/0/$ns3::PacketSink/Rx", MakeBoundCallback(&goodputFunc, yeahGoodPutFP,time_of_start));
	Config::Connect("/NodeList/5/$ns3::Ipv4L3Protocol/Rx", MakeBoundCallback(&throughputFunc, yeahThroughPutFP,time_of_start));

  yeahSocket->TraceConnectWithoutContext("Drop", MakeBoundCallback (&packetDropFunc, "yeah"));
  yeahSocket->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback (&cwndFunc, yeahCongestionWindowFP,time_of_start));
  //-----------------------------------------------------------------------------------------------------

  //-----------------------------------------------------------------------------------------------------
  Ptr<Socket> hyblaSocket = FlowX(InetSocketAddress( ip_hybla6r2.GetAddress(0), portNumber), node.Get(1), node.Get(6), "hybla",time_of_start+50);

	Config::Connect("/NodeList/6/ApplicationList/0/$ns3::PacketSink/Rx", MakeBoundCallback(&goodputFunc, hyblaGoodPutFP,time_of_start));
	Config::Connect("/NodeList/6/$ns3::Ipv4L3Protocol/Rx", MakeBoundCallback(&throughputFunc, hyblaThroughPutFP,time_of_start));

  hyblaSocket->TraceConnectWithoutContext("Drop", MakeBoundCallback (&packetDropFunc, "hybla"));
  hyblaSocket->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback (&cwndFunc, hyblaCongestionWindowFP,time_of_start));
  //-----------------------------------------------------------------------------------------------------

  //-----------------------------------------------------------------------------------------------------
  Ptr<Socket> westwoodSocket = FlowX(InetSocketAddress( ip_westwood7r2.GetAddress(0), portNumber), node.Get(2), node.Get(7), "westwood",time_of_start+50);

	Config::Connect("/NodeList/7/ApplicationList/0/$ns3::PacketSink/Rx", MakeBoundCallback(&goodputFunc, westwoodGoodPutFP,time_of_start));
	Config::Connect("/NodeList/7/$ns3::Ipv4L3Protocol/Rx", MakeBoundCallback(&throughputFunc, westwoodThroughPutFP,time_of_start));

  westwoodSocket->TraceConnectWithoutContext("Drop", MakeBoundCallback (&packetDropFunc, "westwood"));
  westwoodSocket->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback (&cwndFunc, westwoodCongestionWindowFP,time_of_start));
  //-----------------------------------------------------------------------------------------------------

  time_of_start += run_duration;

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

	FlowMonitorHelper flowMonitorHelper;
	Ptr<FlowMonitor> flowMonitorPtr = flowMonitorHelper.InstallAll();

 	Simulator::Stop(Seconds(time_of_start+50));
	Simulator::Run();

  	flowMonitorPtr->CheckForLostPackets();

	Ptr<Ipv4FlowClassifier> ipv4FlowClassifier = DynamicCast<Ipv4FlowClassifier>(flowMonitorHelper.GetClassifier());
	std::map<FlowId, FlowMonitor::FlowStats> mapStatistics = flowMonitorPtr->GetFlowStats();
  std::cout<<"\n";
	for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = mapStatistics.begin(); i != mapStatistics.end(); ++i) {
		Ipv4FlowClassifier::FiveTuple tempClassifier = ipv4FlowClassifier->FindFlow(i->first);

	    if (tempClassifier.sourceAddress=="172.16.111.1"){
	        std::cout << (std::to_string( i->first)).c_str() << " TCP Yeah flow "  <<  tempClassifier.sourceAddress << " to " << tempClassifier.destinationAddress << std::endl;
	        std::cout << "Total Packets Received = " << (std::to_string(  totalPackets- i->second.lostPackets )).c_str() << std::endl;
	        std::cout << "Packet Lost -> Buffer Overflow = " << (std::to_string(mapPacketloss["yeah"])).c_str() << " Congestion = " << (std::to_string( i->second.lostPackets - mapPacketloss["yeah"] )).c_str() << std::endl;
	        std::cout <<"ThroughPut = "<< maxthroughPut["/NodeList/5/$ns3::Ipv4L3Protocol/Rx"] << " kbps" << std::endl;
	    	}
	    else if(tempClassifier.sourceAddress=="172.16.112.1"){
	       std::cout << (std::to_string( i->first)).c_str() << " TCP hybla flow "  <<  tempClassifier.sourceAddress << " to " << tempClassifier.destinationAddress << std::endl;
	        std::cout << "Total Packets Received = " << (std::to_string(  totalPackets- i->second.lostPackets )).c_str() << std::endl;
	        std::cout << "Packet Lost -> Buffer Overflow = " << (std::to_string(mapPacketloss["hybla"])).c_str() << " Congestion = " << (std::to_string( i->second.lostPackets - mapPacketloss["hybla"] )).c_str() << std::endl;
	        std::cout <<"ThroughPut = "<< maxthroughPut["/NodeList/6/$ns3::Ipv4L3Protocol/Rx"] << " kbps" << std::endl;
	      }
	    else if(tempClassifier.sourceAddress=="172.16.113.1"){
	        std::cout << (std::to_string( i->first)).c_str() << " TCP westwood flow "  <<  tempClassifier.sourceAddress << " to " << tempClassifier.destinationAddress << std::endl;
	        std::cout << "Total Packets Received = " << (std::to_string(  totalPackets- i->second.lostPackets )).c_str() << std::endl;
	        std::cout << "Packet Lost -> Buffer Overflow = " << (std::to_string(mapPacketloss["westwood"])).c_str() << " Congestion = " << (std::to_string( i->second.lostPackets - mapPacketloss["westwood"] )).c_str() << std::endl;
	        std::cout <<"ThroughPut = "<< maxthroughPut["/NodeList/7/$ns3::Ipv4L3Protocol/Rx"] << " kbps" << std::endl;
	      }
        std::cout<<"\n";
	}

	Simulator::Destroy();
	return 0;

}
