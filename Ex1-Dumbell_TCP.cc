#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"


using namespace ns3;

// Cubic ou NewReno
std::string tcp_type = "Cubic";
// Arquivo simples para exportar os dados
std::ofstream cwnd_file("dados_cwnd_" + tcp_type + ".txt");
std::ofstream transfer_file("dados_vazao_" + tcp_type + ".txt");
uint64_t lastTotalRx = 0;

// Função que calcula a vazão a cada 0.1s
void CheckThroughput(Ptr<PacketSink> sink) {
    double now = Simulator::Now().GetSeconds();
    uint64_t totalRx = sink->GetTotalRx(); // Total de bytes recebidos até agora
    
    // Calcula quantos Mb chegaram nos últimos 0.1s e converte para Mbps
    double Mbps = (totalRx - lastTotalRx) * 8.0 / 1e6 / 0.1;
    
    transfer_file << now << " " << Mbps << std::endl;
    lastTotalRx = totalRx;
    
    // Agenda a próxima medição para daqui a 0.1s
    Simulator::Schedule(Seconds(0.1), &CheckThroughput, sink);
}

// Função simples: toda vez que a janela mudar, escreve "tempo valor" no arquivo
void RegistraCwnd(uint32_t oldVal, uint32_t newVal) {
    cwnd_file << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
}

// Essa função evita o erro fatal: ela só conecta o rastreador após o TCP começar
void ConectarTrace() {
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback(&RegistraCwnd));
}

int main() {
    /* Preparação */
    // Configura tipo de TCP:
    // - Configura TCP padrão como Reno
    //Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName("ns3::TcpNewReno")));

    // - Configura TCP padrão como Cubic ou NewReno
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName("ns3::Tcp" + tcp_type)));
    std::cout << "[INFO] Configuração do TCP ajustado para " << tcp_type << std::endl;

    // Limita janela TCP (5Mbps):
    // 40ms + 20ms + 40ms (atrasos) = 100ms por OWD (One-Way Delay), 200ms por RTT
    // Portanto, se o limite é 5Mbits/s então, 1Mbits/200ms, ou seja, 125000 bytes
    uint32_t tcp_limit = 125000;
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(tcp_limit));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(tcp_limit));
    std::cout << "[INFO] Banda do TCP ajustado para " << tcp_limit << " bytes ~ " << tcp_limit/25000 << " Mbps"<< std::endl;

    // Tempo de simulação:
    float tempo_final = 200.0;
    std::cout << "[INFO] Duração da simulação: " << tempo_final << " segundos." << std::endl;

    // Quantidade de emissores e receptores:
    uint32_t n_emissores = 2;
    uint32_t n_receptores = 2;
    uint32_t n_roteadores = 2;

    /* Cria dispositivos da rede: emissores, receptores e roteadores */
    NodeContainer emissores_nodes;
    NodeContainer receptores_nodes;
    NodeContainer roteadores_nodes;

    emissores_nodes.Create(n_emissores);
    receptores_nodes.Create(n_receptores);
    roteadores_nodes.Create(n_roteadores);
    std::cout << "[INFO] " << n_emissores << " emissores, " << n_receptores << " receptores e " << n_roteadores << " roteadores criados com sucesso." << std::endl;

    /* Cria conexões entre dispositivos */
    // Roteadores:
    // - Conexão (Cria placas de rede e canal)
    std::string broadband_limit = "10Mbps";

    PointToPointHelper conexao_roteadores;
    conexao_roteadores.SetDeviceAttribute("DataRate", StringValue(broadband_limit));
    conexao_roteadores.SetChannelAttribute("Delay", StringValue("20ms"));

    // Cria o link (liga as placas de rede e o canal aos dispositivos)
    NetDeviceContainer link_roteadores;
    link_roteadores = conexao_roteadores.Install(roteadores_nodes.Get(0), roteadores_nodes.Get(1));

    // Emissores e receptores:
    // - Cria conexão dos emissores e receptores
    PointToPointHelper conexao_em_rec;
    conexao_em_rec.SetDeviceAttribute("DataRate", StringValue(broadband_limit));
    conexao_em_rec.SetChannelAttribute("Delay", StringValue("40ms"));

    // - Faz o link do emissores com o roteador 1
    std::vector<NetDeviceContainer> link_emissores;
    for (uint32_t i = 0; i < n_emissores; i++) {
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

    std::cout << "[INFO] Rede criada com sucesso." << std::endl;

    /* UDP */
    // Porta UDP:
    uint16_t porta_udp = 5;

    // UDP receptor:
    PacketSinkHelper sink_udp(
        "ns3::UdpSocketFactory",
        Address(InetSocketAddress(receptores_interfaces[1].GetAddress(1), porta_udp))
    );
    ApplicationContainer app_udp_receptor = sink_udp.Install(receptores_nodes.Get(1));
    app_udp_receptor.Start(Seconds(0.0));
    app_udp_receptor.Stop(Seconds(tempo_final));
    
    // UDP emissor:
    OnOffHelper udp_background(
        "ns3::UdpSocketFactory",
        Address(InetSocketAddress(receptores_interfaces[1].GetAddress(1), porta_udp))
    );
    std::string udp_limit_string = "8Mbps";
    udp_background.SetAttribute("DataRate", StringValue(udp_limit_string));
    udp_background.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer app_udp_emissor = udp_background.Install(emissores_nodes.Get(1));
    app_udp_emissor.Start(Seconds(0.0));
    app_udp_emissor.Stop(Seconds(tempo_final));

    std::cout << "[INFO] Tráfego UDP de " << udp_limit_string << " criado com sucesso." << std::endl;

    /* TCP */
    // Porta TCP:
    u_int16_t porta_tcp = 10;

    // TCP Receptor:
    PacketSinkHelper tcp_server(
        "ns3::TcpSocketFactory",
        Address(InetSocketAddress(receptores_interfaces[0].GetAddress(1), porta_tcp))
    );
    ApplicationContainer app_tcp_receptor = tcp_server.Install(receptores_nodes.Get(0));
    app_tcp_receptor.Start(Seconds(0.0));
    app_tcp_receptor.Stop(Seconds(tempo_final));

    // TCP Emissor:
    BulkSendHelper tcp_client(
        "ns3::TcpSocketFactory",
        Address(InetSocketAddress(receptores_interfaces[0].GetAddress(1), porta_tcp))
    );
    tcp_client.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer app_tcp_emissor = tcp_client.Install(emissores_nodes.Get(0));
    app_tcp_emissor.Start(Seconds(0.5));
    app_tcp_emissor.Stop(Seconds(tempo_final));

    std::cout << "[INFO] Tráfego TCP " << tcp_type << " criado com sucesso." << std::endl;

    // Mobilidade para o NetAnim
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.InstallAll();

    Simulator::Schedule(Seconds(0.6), &ConectarTrace);
    Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(app_tcp_receptor.Get(0));
    Simulator::Schedule(Seconds(0.1), &CheckThroughput, sink1);

    // NetAnim
    AnimationInterface anim("dumbbell_animation.xml");
    anim.SetMaxPktsPerTraceFile(2000000);
    // Posições para os Emissores (Lado Esquerdo)
    anim.SetConstantPosition(emissores_nodes.Get(0), 10, 20); // S1
    anim.SetConstantPosition(emissores_nodes.Get(1), 10, 40); // S2

    // Posições para os Roteadores (Centro)
    anim.SetConstantPosition(roteadores_nodes.Get(0), 40, 30); // R1
    anim.SetConstantPosition(roteadores_nodes.Get(1), 60, 30); // R2

    // Posições para os Receptores (Lado Direito)
    anim.SetConstantPosition(receptores_nodes.Get(0), 90, 20); // D1
    anim.SetConstantPosition(receptores_nodes.Get(1), 90, 40); // D2
    
    // Rodar simulação
    Simulator::Stop(Seconds(tempo_final));
    std::cout << "[INFO] SIMULAÇÃO INICIADA..." << std::endl;
    Simulator::Run();
    std::cout << "[INFO] Simulação concluída com sucesso!" << std::endl;

    cwnd_file.close();
    transfer_file.close();

    Simulator::Destroy();

    return 0;
}
