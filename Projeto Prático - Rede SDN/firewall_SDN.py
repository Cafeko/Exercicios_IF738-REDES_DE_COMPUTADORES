from ryu.base import app_manager
from ryu.ofproto import ofproto_v1_3
from ryu.controller import ofp_event
from ryu.controller.handler import CONFIG_DISPATCHER, set_ev_cls
from ryu.controller.handler import MAIN_DISPATCHER
from ryu.lib.packet import packet, ethernet, ipv4, icmp

class RedeFirewall(app_manager.RyuApp):
    OFP_VERSIONS = [ofproto_v1_3.OFP_VERSION]


    def __init__(self, *args, **kargs):
        super(RedeFirewall, self).__init__(*args, **kargs)
        self.mac_to_port = {}
        print("Controlador OpenFlow 1.3 iniciado")
    
    # Roda quando um switch conecta ao controlador.
    @set_ev_cls(ofp_event.EventOFPSwitchFeatures, CONFIG_DISPATCHER)
    def switch_features_handler(self, ev):
        # Switch:
        datapath = ev.msg.datapath
        print(f"🔌 Switch conectado: {datapath.id}")

        # Usados para criar regras OpenFlow.
        parser = datapath.ofproto_parser
        ofproto = datapath.ofproto

        # match vazio = “pega qualquer pacote”
        match = parser.OFPMatch()

        # Ação: manda pacote para controlador;
        actions = [parser.OFPActionOutput(ofproto.OFPP_CONTROLLER, ofproto.OFPCML_NO_BUFFER)]

        # Adiciona regra: para o switch que acabou de conectar, pega qualquer pacote e manda para o controlador.
        self.add_flow(datapath, 0, match, actions)
    

    # Adiciona regras ao switch.
    def add_flow(self, datapath, priority, match, actions):
        parser = datapath.ofproto_parser
        ofproto = datapath.ofproto

        inst = [parser.OFPInstructionActions(ofproto.OFPIT_APPLY_ACTIONS, actions)]

        mod = parser.OFPFlowMod(
            datapath=datapath,
            priority=priority,
            match=match,
            instructions=inst
        )
        datapath.send_msg(mod)
    

    # Roda toda vez que um pacote chega ao controlador; cérebro do controlador. 
    @set_ev_cls(ofp_event.EventOFPPacketIn, MAIN_DISPATCHER)
    def packet_in_handler(self, ev):
        # Informações recebidas:
        msg = ev.msg # mensagem
        datapath = msg.datapath # switch que enviou a mensagem

        # Ferramentas OpenFlow:
        parser = datapath.ofproto_parser
        ofproto = datapath.ofproto

        # Porta de entrada:
        in_port = msg.match['in_port']
        
        # Converte mensagem de bytes para um pacote estruturado.
        pkt = packet.Packet(msg.data)

        # Portocolo da camada ethernet
        eth = pkt.get_protocol(ethernet.ethernet)

        # Ignora se for protocolo interno do Mininet (LLDP)
        if eth.ethertype == 0x88cc:
            return

        # Cria a tabela de endereços para o switch, caso não exista ainda.
        dpid = datapath.id
        self.mac_to_port.setdefault(dpid, {})

        # Obtem endereço MAC de origem e destino
        dst = eth.dst
        src = eth.src

        # Preenche tabela de encaminhamento
        self.mac_to_port[dpid][src] = in_port

        # Firewall:
        ip_pkt = pkt.get_protocol(ipv4.ipv4) # Pacote IP
        icmp_pkt = pkt.get_protocol(icmp.icmp) # Pacote ICMP (ping)

        if ip_pkt and icmp_pkt:
            # Se origem = h1 e destino = h4:
            if ip_pkt.src == "10.0.0.1" and ip_pkt.dst == "10.0.0.4":
                match = parser.OFPMatch(
                    eth_type=0x0800,      # IPv4
                    ip_proto=1,           # ICMP
                    ipv4_src="10.0.0.1",
                    ipv4_dst="10.0.0.4"
                )
                # ação a ser executada:
                actions = []  # 🚫 DROP

                # Adiciona regra.
                self.add_flow(datapath, 10, match, actions)

                print("🚫 REGRA DE DROP INSTALADA (h1 -> h4 ICMP)")
                return

        # Encaminhamento dos pacotes:
        # Se endereço de destino está na tabela de encaminhamento: 
        if dst in self.mac_to_port[dpid]:
            out_port = self.mac_to_port[dpid][dst] # envia para porta de saida indicada.
        # Se não:
        else:
            out_port = ofproto.OFPP_FLOOD # Broadcast (envia para todas as portas)

        # Faz ação que foi definida acima (var para porta especifica ou faz broadcast)
        actions = [parser.OFPActionOutput(out_port)]

        # Define quando essa ação será feita.
        match = parser.OFPMatch(in_port=in_port, eth_src=src, eth_dst=dst)

        # Só adiciona regra se não foi um broadcast.
        if out_port != ofproto.OFPP_FLOOD:
            self.add_flow(datapath, 1, match, actions)
        
        # Enviando pacotes:
        # Instruções de envio
        out = parser.OFPPacketOut(
            datapath=datapath,
            buffer_id=ofproto.OFP_NO_BUFFER,
            in_port=in_port,
            actions=actions,
            data=msg.data
        )
        
        # Manda instruções para o switch executar.
        datapath.send_msg(out)