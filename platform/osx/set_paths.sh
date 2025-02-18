base=$HOME/workspace
env=$HOME/osx-environment/x86_64/10.10
sdk=$HOME/SDK

export CPPFLAGS= LDFLAGS="-L$base/lib -L$env/lib -isysroot $sdk/MacOSX11.0.sdk -arch x86_64"
export LINKFLAGS="-L$base/lib -L$env/lib -isysroot $sdk/MacOSX11.0.sdk -arch x86_64"
export MACOSX_DEPLOYMENT_TARGET=10.10
export CXXFLAGS="-I$base/include -I$env/include -isysroot $sdk/MacOSX11.0.sdk -arch x86_64"
export CFLAGS="-I$base/include -I$env/include -isysroot $sdk/MacOSX11.0.sdk -arch x86_64"
export PATH=$env/bin:$PATH
export PKG_CONFIG_PATH=$env/lib/pkgconfig:$base/lib/pkgconfig
