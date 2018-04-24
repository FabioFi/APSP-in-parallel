#include <random> // mt19937_64, uniform_x_distribution
#include <boost/config.hpp>
#include <vector>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/johnson_all_pairs_shortest.hpp>

//using namespace std;
using namespace boost;

typedef adjacency_list<listS, vecS, directedS,
                        no_property, property<edge_weight_t, int> > Graph;
typedef graph_traits<Graph>::vertex_descriptor Vertex;
typedef std::pair<int,int> Edge;
typedef struct graph {
  int V;
  int E;
  Edge *edge_array;
  int *weights;
} graph_t;

inline graph_t *johnson_init(const int n, const double p, const unsigned long seed) {
  static std::uniform_real_distribution<double> flip(0, 1);
  static std::uniform_int_distribution<int> choose_weight(1, 100);

  std::mt19937_64 rand_engine(seed);

  int *adj_matrix = new int[n * n];
  size_t E = 0;
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      if (i == j) {
        adj_matrix[i*n + j] = 0;
      } else if (flip(rand_engine) > p) {
        adj_matrix[i*n + j] = choose_weight(rand_engine);
        E ++;
      } else {
        adj_matrix[i*n + j] = INT_MAX;
      }
    }
  }
  Edge *edge_array = new Edge[E];
  int *weights = new int[E];
  int ei = 0;
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      if (adj_matrix[i*n + j] != 0
          && adj_matrix[i*n + j] != INT_MAX) {
        edge_array[ei] = Edge(i,j);
        weights[ei] = adj_matrix[i*n + j];
        ei++;
      }
    }
  }

  delete[] adj_matrix;

  graph_t *gr = new graph_t;
  gr->V = n;
  gr->E = E;
  gr->edge_array = edge_array;
  gr->weights = weights;

  return gr;
}

void free_graph(graph_t* g) {
  delete[] g->edge_array;
  delete[] g->weights;
}


bool bellman_ford(graph_t* gr, int* dist, int src) {
  int V = gr->V;
  int E = gr->E;
  Edge* edges = gr->edge_array;
  int* weights = gr->weights;

  for (int i = 0; i < V; i++) {
    dist[i] = INT_MAX;
  }
  dist[src] = 0;

  for (int i = 1; i <= V-1; i++) {
    for (int j = 0; j < E; j++) {
      int u = std::get<0>(edges[j]);
      int v = std::get<1>(edges[j]);
      int weight = weights[i];
      std::cerr << u << " , " << v << " -- " << weight << "\n";
      if (dist[u] != INT_MAX && dist[u] + weight < dist[v])
        dist[v] = dist[u] + weight;
    }
  }

  for (int i = 0; i < E; i++) {
    int u = std::get<0>(edges[i]);
    int v = std::get<1>(edges[i]);
    int weight = weights[i];
    if (dist[u] != INT_MAX && dist[u] + weight < dist[v])
        return false;
  }

  return true;
}

void johnson_parallel(graph_t *gr, int* output) {

  // Make new graph for Bellman-Ford
  // First, a new node q is added to the graph, connected by zero-weight edges
  // to each of the other nodes.
  graph_t *bf_graph = new graph_t;
  bf_graph->V = gr->V + 1;
  bf_graph->E = gr->E + gr->V;
  bf_graph->edge_array = new Edge[bf_graph->E];
  bf_graph->weights = new int[bf_graph->E];

  std::cerr << "Mem cpy to graph : " << bf_graph->V << " , " << bf_graph->E << "\n";
  std::memcpy(bf_graph->edge_array, gr->edge_array, gr->E  * sizeof(Edge));
  std::memcpy(bf_graph->weights, gr->weights, gr->E * sizeof(int));
  std::memset(&bf_graph->weights[gr->E], 0, gr->V * sizeof(int));

  // TODO parallel for
  std::cerr << "Adding Edges to graph\n";
  for (int e = gr->E; e < bf_graph->E; e++) {
    bf_graph->edge_array[e] = Edge(gr->V, e);
  }

  //Graph BFG(bf_graph->edge_array, bf_graph->edge_array + bf_graph->E,
  //          bf_graph->weights, bf_graph->V);

  //std::vector<int> h(num_vertices(BFG), (std::numeric_limits < short >::max)());
  //std::cerr << "Creating h vector of size " << h.size() << "__" << num_vertices(BFG)
  //    << "\n";
  // Second, the Bellman–Ford algorithm is used, starting from the new vertex q,
  // to find for each vertex v the minimum weight h(v) of a path from q to v. If
  // this step detects a negative cycle, the algorithm is terminated.
  // TODO Can run parallel version?
  std::cerr << "Bellman ford\n";
  //bool r = bellman_ford_shortest_paths(BFG, 0, distance_map(&h[0]));
  int* h = new int[bf_graph->V];
  bool r = bellman_ford(bf_graph, h, gr->V);
  std::cerr << "Bellman ford sucess\n";
  if (!r) {
    std::cerr << "Negative Cycles Detected! Terminating Early\n";
    return;
  }
  // Next the edges of the original graph are reweighted using the values computed
  // by the Bellman–Ford algorithm: an edge from u to v, having length
  // w(u,v), is given the new length w(u,v) + h(u) − h(v).
  // TODO parallel for
  std::cerr << "reweighting\n";
  for (int e = 0; e < gr->E; e++) {
    int u = std::get<0>(gr->edge_array[e]);
    int v = std::get<1>(gr->edge_array[e]);
    std::cerr << "(" << u << "," << v << ") " << h[u] << "-" << h[v] << "\n";
    gr->weights[e] = gr->weights[e] + h[u] - h[v];
  }

  Graph G(gr->edge_array, gr->edge_array + gr->E, gr->weights, gr->V);

  std::cerr << "Dijkstra\n";
  // TODO parallel for
  for (int s = 0; s < gr->V; s++) {
    std::vector<int> d(num_vertices(G));
    dijkstra_shortest_paths(G, s, distance_map(&d[0]));
    for (int v = 0; v < gr->V; v++) {
      //std::cerr << d[v] << " - " << h[v] << ", " << h[s] << "\n";
      output[s*gr->V] = d[v] + h[v] - h[s];
    }
  }
}
