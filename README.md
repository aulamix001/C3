# NX Audio Pro C3

Compressor VST3 mono e estéreo em C++17 e JUCE 8. O plug-in aparece no host como **NX PRO C 3**. A interface usa desenho vetorial, placas grafite translúcidas, knobs metálicos com arcos dourados, histórico de áudio e três medidores verticais.

## Controles

- **Threshold** vai de −60 a 0 dB. **Ratio** vai de 1:1 a 20:1. O modo **Clean** aplica uma curva de compressão com knee quadrático contínuo, seguida de um envelope em dB.
- **Attack** (0,05–250 ms) e **Release** (5–2.000 ms) são constantes de tempo de um envelope de primeira ordem. Após uma constante de tempo, o envelope percorre cerca de 63% da distância até o alvo. **Auto Release** usa 45% do tempo configurado quando a redução cai abaixo de 8 dB.
- **Knee**, **Range** e **Hold** ajustam a largura da transição, o limite máximo da redução e o tempo de retenção antes da liberação.
- **Lookahead** antecipa a redução entre 0 e 10 ms. O plug-in informa ao host uma latência fixa de 10 ms, inclusive em bypass. A antecipação seleciona um ponto interpolado no histórico da redução, e os caminhos seco, comprimido e de escuta permanecem alinhados.
- **Auto Gain** acrescenta metade da redução estática prevista para um sinal de referência de −18 dBFS, limitada a +6 dB e suavizada em 120 ms. **Makeup** é um ajuste independente de −24 a +24 dB. **Dry** mostra o complemento do parâmetro legado Mix.
- Para usar **EXT**, habilite e roteie a entrada auxiliar **Sidechain** na DAW. O detector volta à entrada principal se o barramento auxiliar estiver desabilitado. **HP** e **LP** são filtros de primeira ordem do detector. Na posição máxima, LP fica totalmente aberto. **Listen** permite ouvir o detector filtrado. **Peak** usa a amplitude instantânea e **RMS** usa a energia média exponencial com constante de 10 ms. **Link** interpola a detecção estéreo independente com o máximo dos canais.
- Os medidores capturam picos por bloco em estruturas atômicas e atualizam a 30 Hz. A queda visual é de 24 dB/s, o clipping indica um pico de amostra a partir de 0 dBFS e permanece visível por cerca de 1 segundo. O peak hold oferece 0, 1,5 ou 3 segundos. O histórico cobre 7,5 segundos e oferece escalas de 36, 48 e 72 dB.
- Arraste verticalmente, segure Shift antes de arrastar para ajuste fino, use a roda para incrementar, clique no valor para digitar e dê duplo clique no knob para restaurar o padrão. Os dez presets de fábrica e os IDs de automação da versão anterior foram preservados. A DAW salva a configuração ativa. **A/B** compara dois ajustes enquanto o editor está aberto. **Copy A/B** copia o lado ativo para o outro.

O motor usa double para o processamento de entradas float e double. A memória de atraso é reservada em `prepareToPlay`. Os controles contínuos são interpolados em 15 ms e bypass e Listen fazem transições de 10 ms. O motor é linear em relação ao sinal para um ganho fixo, sem estágio de saturação, e opera na taxa de amostragem da sessão.

## Compilar e testar

Use CMake 3.22 ou posterior e um compilador C++17. O CMake baixa as revisões fixadas de JUCE e doctest com SHA-256. No Windows, use o Visual Studio 2022. No Linux, instale as dependências de desenvolvimento de ALSA, FreeType, Fontconfig, OpenGL, X11, Xext, Xrandr, Xinerama, Xcursor e Xcomposite.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target NXProC3_VST3 NXCompressorTests
ctest --test-dir build -C Release --output-on-failure
```

No Linux, configure com `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release` antes de executar o build e o CTest. Os testes cobrem a curva estática, knee, attack, release, range, hold, antecipação, atraso e bypass, mix paralelo, sidechain externo e filtragem de escuta, estabilidade e automação, precisão float/double, resposta e THD a ganho unitário, três taxas de amostragem e restauração de presets antigos e novos.

O GitHub Actions executa o build x64, CTest e pluginval nível 10 no Windows. Tags no formato `vX.Y.Z` geram uma release com o ZIP. Para instalar localmente, copie a pasta `NX PRO C 3.vst3` inteira de `build/NXProC3_artefacts/Release/VST3/` para `C:\Program Files\Common Files\VST3` no Windows ou `~/.vst3/` no Linux, e atualize a lista de plug-ins da DAW.

## Comparação visual

Com uma sessão gráfica disponível, execute o CTest com `NX_CAPTURE_DIR` definido para uma pasta de saída. O teste alimenta o plug-in com áudio, gera PNGs compactos e grandes e renderiza capturas a 100%, 125%, 150% e 200%. Ele também verifica a renderização do editor com 7.680 pixels de largura.

Na comparação com a referência, recorte a moldura do DAW e compare a área interna do plug-in em escala normalizada. Confira a ocupação do gráfico, a legibilidade da régua, o alinhamento dos quatro knobs principais, a espessura dos arcos, a separação entre valores e legendas, e o contraste entre os painéis arredondados e a placa de fundo. A composição translúcida é do editor. A transparência em relação à janela do DAW depende de como o host incorpora o VST3.
