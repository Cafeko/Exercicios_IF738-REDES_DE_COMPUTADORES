#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/applications-module.h"


using namespace ns3;


int main() {
    /* Preparação */
    // Configura tipo de TCP:
    // - Configura TCP padrão como Reno
    //Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName("ns3::TcpNewReno")));

    // - Configura TCP padrão como Cubic
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName("ns3::TcpCubic")));
    
    // Limita janela TCP (5Mbps):
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(125000));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(125000));

    // Tempo de simulação:
    float tempo_final = 10.0;

    // Quantidade de emissores e receptores:
    uint32_t n_emissores = 2;
    uint32_t n_receptores = 2;


    /* Cria dispositivos da rede: emissores, receptores e roteadores */
    NodeContainer emissores_nodes;
    NodeContainer receptores_nodes;
    NodeContainer roteadores_nodes;

    emissores_nodes.Create(n_emissores);
    receptores_nodes.Create(n_receptores);
    roteadores_nodes.Create(2);


    /* Cria conecções entre dispositivos */
    // Roteadores:
    // - Conexão (Cria placas de rede e canal)
    PointToPointHelper conexao_roteadores;
    conexao_roteadores.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    conexao_roteadores.SetChannelAttribute("Delay", StringValue("20ms"));

    // Cria o link (liga as placas de rede e o canal aos dispositivos)
    NetDeviceContainer link_roteadores;
    link_roteadores = conexao_roteadores.Install(roteadores_nodes.Get(0), roteadores_nodes.Get(1));

    // Emissores e receptores:
    // - Cria conexão dos emissores e receptores
    PointToPointHelper conexao_em_rec;
    conexao_em_rec.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    conexao_em_rec.SetChannelAttribute("Delay", StringValue("40ms"));

    // - Faz o link do emissores com o roteador 1
    std::vector<NetDeviceContainer> link_emissores;
    for (uint32_t i = 0; i < n_emissores; i++){
        link_emissores.push_back(conexao_em_rec.Install(emissores_nodes.Get(i), roteadores_nodes.Get(0)));
    }

    // - Faz o link do roteador 2 com os receptores
    std::vector<NetDeviceContainer> link_receptores;
    for (uint32_t i = 0; i < n_receptores; i++) {
        link_receptores.push_back(conexao_em_rec.Install(roteadores_nodes.Get(1), receptores_nodes.Get(i)));
    }


    /* Endereçamento dos dispositivos */
    // Instalando pilhas de protocolos de internet em todos os nodes:
    InternetStackHelper stack;
    stack.InstallAll();

    // Set endereços IP:
    Ipv4AddressHelper address;
    
    // - Rede entre Roteadores
    address.SetBase("10.0.0.0", "255.255.255.0");
    address.Assign(link_roteadores);

    // - Rede entre emissores e roteador 1
    std::vector<Ipv4InterfaceContainer> emissores_interfaces;
    for (uint32_t i = 0; i < n_emissores; i++) {
        std::string subnet = "10.1." + std::to_string(i) + ".0";
        address.SetBase(subnet.c_str(), "255.255.255.0");
        Ipv4InterfaceContainer iface_emissores = address.Assign(link_emissores[i]);
        emissores_interfaces.push_back(iface_emissores);
    }

    // - Rede entre roteador 2 e receptores
    std::vector<Ipv4InterfaceContainer> receptores_interfaces;
    for (uint32_t i = 0; i < n_receptores; i++) {
        std::string subnet = "10.2." + std::to_string(i) + ".0";
        address.SetBase(subnet.c_str(), "255.255.255.0");
        Ipv4InterfaceContainer iface_receptores = address.Assign(link_receptores[i]);
        receptores_interfaces.push_back(iface_receptores);
    }

    // Faz roteamento automaticamente:
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();


    /* UDP */
    // Porta UDP:
    uint16_t porta_udp = 5;

    // UDP receptor:
    PacketSinkHelper sink_udp(
        "ns3::UdpSocketFactory",
        Address(InetSocketAddress(emissores_interfaces[1].GetAddress(0), porta_udp))
    );
    ApplicationContainer app_udp_receptor = sink_udp.Install(receptores_nodes.Get(1));
    app_udp_receptor.Start(Seconds(0.0));
    app_udp_receptor.Stop(Seconds(tempo_final));
    
    // UDP emissor:
    OnOffHelper udp_background(
        "ns3::UdpSocketFactory",
        Address(InetSocketAddress(receptores_interfaces[1].GetAddress(1), porta_udp))
    );
    udp_background.SetAttribute("DataRate", StringValue("8Mbps"));
    udp_background.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer app_udp_emissor = udp_background.Install(emissores_nodes.Get(1));
    app_udp_emissor.Start(Seconds(0.0));
    app_udp_emissor.Stop(Seconds(tempo_final));


    /* TCP */
    // Porta TCP:
    u_int16_t porta_tcp = 10;

    // TCP Receptor:
    PacketSinkHelper tcp_server(
        "ns3::TcpSocketFactory",
        Address(InetSocketAddress(emissores_interfaces[0].GetAddress(0), porta_tcp))
    );
    ApplicationContainer app_tcp_receptor = tcp_server.Install(receptores_nodes.Get(0));
    app_tcp_receptor.Start(Seconds(0.0));
    app_tcp_receptor.Stop(Seconds(tempo_final));

    // TCP Emissor:
    BulkSendHelper tcp_client(
        "ns3::TcpSocketFactory",
        Address(InetSocketAddress(receptores_interfaces[0].GetAddress(0), porta_tcp))
    );
    tcp_client.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer app_tcp_emissor = tcp_client.Install(emissores_nodes.Get(0));
    app_tcp_emissor.Start(Seconds(0.0));
    app_tcp_emissor.Stop(Seconds(tempo_final));


    /* Rodar simulação */
    Simulator::Stop(Seconds(tempo_final));
    Simulator::Run();
    Simulator::Destroy();
}
