from mininet.net import Mininet
from mininet.node import RemoteController, OVSSwitch
from mininet.cli import CLI
from mininet.link import TCLink

def rede():
    # Cria a rede:
    print("*** Criando rede")
    net = Mininet(controller=None, switch=OVSSwitch, link=TCLink)

    # Controlador:
    print("*** Adicionando controlador remoto a rede")
    controller = net.addController(
        'c0',
        controller=RemoteController,
        ip='127.0.0.1',
        port=6653
    )

    # Hosts:
    print("*** Criando hosts")
    h1 = net.addHost("h1", ip="10.0.0.1/24")
    h2 = net.addHost("h2", ip="10.0.0.2/24")
    h3 = net.addHost("h3", ip="10.0.0.3/24")
    h4 = net.addHost("h4", ip="10.0.0.4/24")

    # Switches:
    print("*** Criando switches")
    s1 = net.addSwitch("s1", protocols='OpenFlow13')
    s2 = net.addSwitch("s2", protocols='OpenFlow13')

    # Links:
    print("*** Criando links")
    net.addLink(s1, s2, cls=TCLink, bw=100)
    net.addLink(h1, s1)
    net.addLink(h2, s1)
    net.addLink(h3, s2)
    net.addLink(h4, s2)

    # Rodando rede:
    print("*** Iniciando rede")
    net.start()
    print("*** CLI")
    CLI(net)
    print("*** Parando rede")
    net.stop()

if __name__ == "__main__":
    rede()