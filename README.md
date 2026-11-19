# NX PRO C 3

Compressor VST3 mono e estéreo, desenvolvido em C++17 com JUCE 8. A interface vetorial redimensionável usa painéis translúcidos, histórico de sinal, curva de transferência e medidores de entrada, saída e redução de ganho.

## Compilar

Use CMake 3.22 ou posterior, Ninja e um compilador C++17. Em Linux, instale as dependências de desenvolvimento de ALSA, FreeType, Fontconfig, OpenGL, X11, Xext, Xrandr, Xinerama, Xcursor e Xcomposite. O CMake obtém a revisão fixada do JUCE no GitHub.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target NXProC3_VST3
```

No Linux, copie o pacote inteiro `NX PRO C 3.vst3`, encontrado em `build/NXProC3_artefacts/Release/VST3/`, para `~/.vst3/` e atualize a lista de plug-ins da DAW.

## Usar

- Ajuste limiar, razão, ataque e release em **Dynamics**. Clique duas vezes em um controle para restaurar seu valor inicial.
- Use **Shaping & Detector** para ajustar knee, faixa de redução, hold, makeup, mix, passa-altas do detector e vínculo estéreo.
- Para usar sidechain externo, habilite e roteie a entrada **Sidechain** na DAW e selecione **External key**. Sem um barramento externo habilitado, o detector usa a entrada principal. **Listen** envia o sinal filtrado do detector para a saída.
- Use o menu de presets para escolher um dos dez pontos de partida agrupados por instrumento. A DAW salva os parâmetros na sessão.
- A janela preserva a proporção 3:2 e pode ser redimensionada até 7.680 pixels de largura, conforme os limites da DAW e da tela.
