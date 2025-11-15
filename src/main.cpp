#include "json_editor.hpp"

#include <string>
#include <iostream>
#include <fstream>

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <filename.json>" << std::endl;
    return EXIT_FAILURE;
  }

  std::ifstream input_file(argv[1]);
  if (!input_file) {
    std::cerr << "Error: Could not open file " << argv[1] << std::endl;
    return EXIT_FAILURE;
  }

  json input_json;
  try {
    input_file >> input_json;
  } catch (json::exception& e) {
    std::cerr << "Error parsing JSON: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  input_file.close(); 

  auto screen = ScreenInteractive::Fullscreen();
  JsonEditor editor(input_json, argv[1]);

  auto custom_loop = [&] {
    try {
      screen.Loop(editor.GetLayout());
    } catch (...) {}
    std::cout << "\nSaving changed to " << argv[1] << "..." << std::endl;
    std::ofstream output_file(argv[1]);
    if (!output_file) {
      std::cerr << "Error: Could not open file" << argv[1] << " for writing." << std::endl;
      return EXIT_FAILURE;
    }
    try {
      output_file << input_json.dump(2);
      output_file.close();
      std::cout << "Done." << std::endl;
    } catch (json::exception& e) {
      std::cerr << "Error saving JSON: " << e.what() << std::endl;
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  };
  
  return custom_loop();
}
