/*
Author: Liam Nayyar
Date: 2026-03-27
Purpose: The file to store the logic/functions for the creation of a custom black hole with various characteristics.
This would only be for research, simulation, and data collection purposes, with the goal to better model our understanding of black holes and their properties.
*/
#ifndef CUSTOM_BLACK_HOLE_HPP
#define CUSTOM_BLACK_HOLE_HPP
#include "black_hole.hpp"
    // function to show the custom black hole creation menu and handle user input after the toggle key "j" is pressed
void custom_black_hole_menu() {
    std::cout << "Custom Black Hole Creation Menu" << std::endl;
    std::cout << "Enter the mass of the black hole (in solar masses): ";
    double mass;
    std::cin >> mass;   
    std::cout << "Enter the spin of the black hole (0 to 1): ";
    double spin;
    std::cin >> spin;
    std::cout << "Enter the charge of the black hole (in Coulombs): ";
    double charge;
    std::cin >> charge;
    // Create the custom black hole with the specified parameters
    BlackHole customBlackHole(mass, spin, charge);
    std::cout << "Custom black hole created with mass: " << mass << " solar masses, spin: " << spin << ", charge: " << charge << " Coulombs." << std::endl;
    // Additional logic to add the custom black hole to the simulation or visualization can be implemented here
} // a lot more to come 

#endif // CUSTOM_BLACK_HOLE_HPP
