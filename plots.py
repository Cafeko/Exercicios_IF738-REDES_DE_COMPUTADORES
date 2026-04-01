import matplotlib.pyplot as plt

# Cubic ou NewReno
tcp_type = ["Cubic", "NewReno"]

for tcp_t in tcp_type:

    tempos = []
    janelas = []
    vazao = []

    # Lê o arquivo exportado pelo C++
    with open("dados_cwnd_" + tcp_t + ".txt", "r") as f:
        for linha in f:
            t, v = linha.split()
            tempos.append(float(t))
            janelas.append(float(v))

    plt.figure(figsize=(10, 5))
    plt.plot(tempos, janelas, label='Janela de Congestionamento (CWND)')
    plt.title('Evolução da Janela TCP ' + tcp_t + ' no Tempo')
    plt.xlabel('Tempo (s)')
    plt.ylabel('Bytes')
    plt.grid(True)
    plt.savefig('grafico_janela_' + tcp_t + '.png')
    print("Gráfico 'grafico_janela_" + tcp_t + ".png' gerado com sucesso!")

    tempos = []
    
    with open("dados_vazao_" + tcp_t + ".txt", "r") as f:
        for linha in f:
            t, v = map(float, linha.split())
            tempos.append(t)
            vazao.append(v)

    plt.figure(figsize=(10, 5))
    plt.plot(tempos, vazao, color='green', linewidth=2)
    plt.title('Taxa de Transferência Real (Throughput) - S1 para D1')
    plt.xlabel('Tempo (s)')
    plt.ylabel('Vazão (Mbps)')
    plt.ylim(0, 10) # O link máximo é 10Mbps
    plt.grid(True)
    plt.savefig('grafico_vazao_' + tcp_t + '.png')
    print("Gráfico 'grafico_vazao_" + tcp_t + ".png' gerado com sucesso!")