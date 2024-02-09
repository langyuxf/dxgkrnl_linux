#/bin/bash

if [ ! -d "$HOME/libdxg" ]; then
        git clone https://github.com/microsoft/libdxg.git $HOME/libdxg
fi

if [ ! -d "$HOME/DirectX-Headers" ]; then
        git clone https://github.com/microsoft/DirectX-Headers.git $HOME/DirectX-Headers
fi

g++ demo.cpp -o demo -I "$HOME/libdxg/include" \
                     -I "$HOME/DirectX-Headers/include" \
                     -I "$HOME/DirectX-Headers/include/wsl/stubs" \
                     -L /usr/lib/wsl/lib -ldxcore

