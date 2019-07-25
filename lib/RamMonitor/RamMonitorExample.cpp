
/*
#include <cstdint>

#include "RamMonitor.h"

RamMonitor ram;
uint32_t   reporttime;

void report_ram_stat(const char* aname, uint32_t avalue) {
  Serial.print(aname);
  Serial.print(": ");
  Serial.print((avalue + 512) / 1024);
  Serial.print(" Kb (");
  Serial.print((((float) avalue) / ram.total()) * 100, 1);
  Serial.println("%)");
};

void report_ram() {
  bool lowmem;
  bool crash;
  
  Serial.println("==== memory report ====");
  
  report_ram_stat("free", ram.adj_free());
  report_ram_stat("stack", ram.stack_total());
  report_ram_stat("heap", ram.heap_total());
  
  lowmem = ram.warning_lowmem();
  crash = ram.warning_crash();
  if(lowmem || crash) {
    Serial.println();
    
    if(crash)
      Serial.println("**warning: stack and heap crash possible");
    else if(lowmem)
      Serial.println("**warning: unallocated memory running low");
  };
  
  Serial.println();
};

void setup() {
  ram.initialize();
  
  while(!Serial);
  
  reportime = millis();
};

void loop() {
  uint32_t time = millis();
  
  if((time - reporttime) > 2000) {
    reporttime = time;
    report_ram();
  };
  
  ram.run();
};


*/