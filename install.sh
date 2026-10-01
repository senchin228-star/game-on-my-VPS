#!/bin/bash

# Detect Linux distribution
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$ID
else
    echo "❌ Failed to detect Linux distribution"
    exit 1
fi

echo "📦 Installing dependencies for $OS..."

case "$OS" in
    ubuntu|debian)
        sudo apt-get update
        sudo apt-get install -y \
            build-essential \
            libsdl2-dev \
            libsdl2-image-dev \
            libsdl2-ttf-dev
        ;;
    
    fedora|rhel|centos)
        sudo dnf install -y \
            gcc \
            make \
            SDL2-devel \
            SDL2_image-devel \
            SDL2_ttf-devel
        ;;
    
    arch|manjaro|cachyos)
        sudo pacman -S --noconfirm \
            base-devel \
            sdl2 \
            sdl2_image \
            sdl2_ttf
        ;;
    
    opensuse*)
        sudo zypper install -y \
            gcc \
            make \
            libSDL2-devel \
            libSDL2_image-devel \
            libSDL2_ttf-devel
        ;;
    
    alpine)
        apk update
        apk add --no-cache \
            build-base \
            sdl2-dev \
            sdl2_image-dev \
            sdl2_ttf-dev
        ;;
    
    *)
        echo "❌ Unsupported distribution: $OS"
        echo "Please install manually:"
        echo "  - gcc/clang"
        echo "  - SDL2 dev packages"
        echo "  - SDL2_image dev packages"
        echo "  - SDL2_ttf dev packages"
        exit 1
        ;;
esac

echo "✅ Dependencies installed!"

# Check for config file
if [ ! -f config.h ]; then
    echo ""
    echo "⚙️  Creating config.h from config.example.h..."
    if [ -f config.example.h ]; then
        cp config.example.h config.h
        echo "✅ config.h created! Edit it if necessary."
    else
        echo "❌ config.example.h not found"
        exit 1
    fi
fi

echo ""
echo "🔨 Building client..."
make client

if [ $? -eq 0 ]; then
    echo "✅ Client built successfully!"
else
    echo "❌ Error building client"
    exit 1
fi
