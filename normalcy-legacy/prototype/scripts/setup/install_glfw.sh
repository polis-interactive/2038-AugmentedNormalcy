
sudo apt-get install libwayland-dev libxkbcommon-dev wayland-protocols extra-cmake-modules libdrm-dev

git clone https://github.com/glfw/glfw

cd glfw

cmake -DGLFW_BUILD_WAYLAND=ON -DGLFW_BUILD_DOCS=OFF -DGLFW_BUILD_X11=OFF ..

make -j4

sudo make install