//
// Created by viking on 2025/3/10.
//
#include "data/dataset.h"
#include "src/observer.h"
#include "src/draw.h"
#include <iostream>

int main() {
//    Tycho2Dataset dataset;
//    dataset.ProcessDirectory("data/Tycho-2");

    observer a(0.0,0.0,10.0,10.0);
    std::vector<star> stars = a.FileterStarInViewMultithreaded(40);
    stars.push_back(star{0,0,3});
    for(auto& star : stars) {
      std::cout<<star.ra<<" "<<star.dec<<" "<<star.magnitude<<std::endl;
    }
    for(const auto& star : stars) {
        std::cout<<star.ra<<" "<<star.dec<<" "<<star.magnitude<<std::endl;
    }

    StarMapDrawer::drawStarMap(stars, "output/output.png", 1024, 1024, 0.0, 0.0, 10.0, 10.0,12.0);
    return 0;

}
