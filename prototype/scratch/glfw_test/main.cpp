
#include <iostream>
#include <signal.h>
#include <cstdio>

#include "graphics.hpp"
#include "camera.hpp"


std::function<void(int)> shutdown_handler;
void signal_handler(int signal) { shutdown_handler(signal); }


int run_test_graphics() {
  freopen( "output.txt", "w", stdout );
    
  std::cout << "Starting glfw thread" << std::endl;

  
  GLFWHandler handler;
  CameraTest t(handler);  
  bool exit = false;

  shutdown_handler = [&](int signal) {
    std::cout << "Server shutdown...\n";
    exit = true;
  };

  signal(SIGINT, signal_handler);
    
  
   while(!exit){
      std::this_thread::sleep_for(1s);
   }
   
   // handler.Close();
   
   std::cout << "Prog exit!" << std::endl;
   
   return 0;
}
  

int main(int /* argc */, char ** /* argv */) {
  std::cout << "Here goes nothing" << std::endl;
  run_test_graphics();
  std::cout << "we made it!" << std::endl;
}
