#include <iostream>
#include <vector>
#include <fstream>

// Function to generate a dense graph
std::vector<std::pair<int, int>> generateDenseGraph(int numNodes) {
    std::vector<std::pair<int, int>> edges;

    // Create a fully connected graph (complete graph)
    for (int i = 1; i <= numNodes; ++i) {
        for (int j = i + 1; j <= numNodes; ++j) {
            edges.emplace_back(i, j); // Add edge (i, j)
        }
    }

    return edges;
}

// Function to write the graph to a file
void writeGraphToFile(const std::vector<std::pair<int, int>>& edges, const std::string& filename) {
    std::ofstream file(filename);
    if (file.is_open()) {
        for (const auto& edge : edges) {
            file << edge.first << " " << edge.second << "\n";
        }
        file.close();
        std::cout << "Graph written to " << filename << std::endl;
    } else {
        std::cerr << "Unable to open file " << filename << std::endl;
    }
}

int main() {
    int numNodes = 100; // Number of nodes (can be scaled up for testing)

    // Generate a dense graph
    auto denseGraph = generateDenseGraph(numNodes);

    // Write the graph to a file for testing
    writeGraphToFile(denseGraph, "dense_graph.txt");

    std::cout << "Number of nodes: " << numNodes << std::endl;
    std::cout << "Number of edges: " << denseGraph.size() << std::endl;

    // Print a few sample edges
    std::cout << "Sample edges:" << std::endl;
    for (size_t i = 0; i < std::min<size_t>(10, denseGraph.size()); ++i) {
        std::cout << denseGraph[i].first << " " << denseGraph[i].second << std::endl;
    }

    return 0;
}

