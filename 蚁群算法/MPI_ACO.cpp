#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string>
#include "ants.h"
#include <math.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>

using namespace std;

struct Ant_t
{
    float tour_length;
    int cur_node, next_node, path_index;
    int path[MAX_NODES];
    int tabu[MAX_NODES];
};

int colony_size;
int *res;
Ant_t *ant_colony;
float pheromone[MAX_NODES][MAX_NODES];
float best = (float)MAX_TOUR;
int best_index;

cityType cities[MAX_NODES];
string graph;
EdgeMatrix *dist;

void initAnt()
{
    for (int from = 0; from < MAX_NODES; from++)
    {
        for (int to = 0; to < MAX_NODES; to++)
        {
            pheromone[from][to] = INIT_PHER;
        }
    }

    for (int ant = 0; ant < colony_size; ant++)
    {
        ant_colony[ant].cur_node = rand() % MAX_NODES;
        for (int to = 0; to < MAX_NODES; to++)
        {
            ant_colony[ant].tabu[to] = 0;
        }
        ant_colony[ant].path_index = 0;
        ant_colony[ant].path[0] = ant_colony[ant].cur_node;
        ant_colony[ant].next_node = -1;
        ant_colony[ant].tour_length = 0;
        ant_colony[ant].tabu[ant_colony[ant].cur_node] = 1;
    }
}

void restartAnts()
{
    for (int ant = 0; ant < colony_size; ant++)
    {
        ant_colony[ant].next_node = -1;
        ant_colony[ant].tour_length = 0.0;
        for (int i = 0; i < MAX_NODES; i++)
        {
            ant_colony[ant].tabu[i] = 0;
            ant_colony[ant].path[i] = -1;
        }
        ant_colony[ant].cur_node = rand() % MAX_NODES;
        ant_colony[ant].path_index = 1;
        ant_colony[ant].path[0] = ant_colony[ant].cur_node;
        ant_colony[ant].tabu[ant_colony[ant].cur_node] = 1;
    }
}

double calculateProbability(int from, int to)
{
    return (double)(pow(pheromone[from][to], ALPHA) * pow((1.0 / (*dist)[from][to]), BETA));
}

int selectNextCity(int ant)
{
    int from = ant_colony[ant].cur_node;
    double sum = 0.0;
    for (int to = 0; to < MAX_NODES; to++)
    {
        if (ant_colony[ant].tabu[to] == 0)
        {
            sum += calculateProbability(from, to);
        }
    }

    int last_best_index = 0.0;
    srand((unsigned)time(NULL));
    double lucky_number = (double)rand() / RAND_MAX;

    for (int to = 0; to < MAX_NODES; to++)
    {
        if (ant_colony[ant].tabu[to] == 0)
        {
            double product = calculateProbability(from, to) / sum;
            if (product > 0)
            {
                lucky_number -= product;
                last_best_index = to;

                if (lucky_number <= 0.0)
                {
                    return to;
                }
            }
        }
    }
    return last_best_index;
}

void updatePheromone(Ant_t *ant_colony)
{
    int from, to, i, ant;
    for (from = 0; from < MAX_NODES; from++)
    {
        for (to = 0; to < from; to++)
        {
            pheromone[from][to] *= 1.0 - RHO;

            if (pheromone[from][to] < 0.0)
            {
                pheromone[from][to] = INIT_PHER;
            }
            pheromone[to][from] = pheromone[from][to];
        }
    }

    for (ant = 0; ant < colony_size; ant++)
    {
        for (i = 0; i < MAX_NODES; i++)
        {
            from = ant_colony[ant].path[i];
            if (i < MAX_NODES - 1)
            {
                to = ant_colony[ant].path[i + 1];
            }
            else
            {
                to = ant_colony[ant].path[0];
            }
            pheromone[from][to] += (QVAL / ant_colony[ant].tour_length);
            pheromone[to][from] = pheromone[from][to];
        }
    }
}

float euclideanDistance(int x1, int x2, int y1, int y2)
{
    int xd = x1 - x2;
    int yd = y1 - y2;
    return (int)(sqrt(xd * xd + yd * yd) + 0.5);
}

float pseudoEuclideanDistance(int x1, int x2, int y1, int y2)
{
    int xd = x1 - x2;
    int yd = y1 - y2;
    float rij = sqrt((xd * xd + yd * yd) / 10.0);
    return ceil(rij);
}

void constructTSP(string graph, cityType *cities, EdgeMatrix *dist)
{
    ifstream infile(("instances/" + graph + ".tsp").c_str());
    string line;
    bool euclidean = true;

    int city;
    float x, y;
    bool reading_nodes = false;

    while (std::getline(infile, line))
    {
        istringstream iss(line);
        string word;
        if (!reading_nodes)
        {
            iss >> word;
            if (word.compare("EDGE_WEIGHT_TYPE") == 0)
            {
                iss >> word >> word;
                euclidean = !word.compare("EUC_2D");
            }
            else if (word.compare("NODE_COORD_SECTION") == 0)
            {
                reading_nodes = true;
            }
        }
        else if (iss >> city >> x >> y)
        {
            cities[city - 1].x = x;
            cities[city - 1].y = y;
        }
    }
    infile.close();

    for (int from = 0; from < MAX_NODES; from++)
    {
        (*dist)[from][from] = 0.0;

        for (int to = from + 1; to < MAX_NODES; to++)
        {
            float edge_dist;
            if (euclidean)
            {
                edge_dist = euclideanDistance(cities[from].x, cities[to].x, cities[from].y, cities[to].y);
            }
            else
            {
                edge_dist = pseudoEuclideanDistance(cities[from].x, cities[to].x, cities[from].y, cities[to].y);
            }
            if (edge_dist == 0)
            {
                edge_dist = 1.0;
            }
            (*dist)[from][to] = edge_dist;
            (*dist)[to][from] = edge_dist;
        }
    }
}

float bestSolution(Ant_t *container)
{
    for (int ant = 0; ant < colony_size; ant++)
    {
        if (container[ant].tour_length < best)
        {
            best = container[ant].tour_length;
            best_index = ant;
        }
    }
    return best, best_index;
}

auto solveTSP()
{
    MPI_Datatype Ant_type;
    int lengths[6] = {1, 1, 1, 1, MAX_NODES, MAX_NODES};
    const MPI_Aint displacements[6] = {0, sizeof(float), sizeof(float) + sizeof(int), sizeof(int) * 2 + sizeof(float), sizeof(int) * 3 + sizeof(float), sizeof(int) * MAX_NODES + sizeof(int) * 3 + sizeof(float)};
    MPI_Datatype types[6] = {MPI_FLOAT, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT};
    MPI_Type_create_struct(6, lengths, displacements, types, &Ant_type);
    MPI_Type_commit(&Ant_type);

    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    char processor_name[MPI_MAX_PROCESSOR_NAME];
    int name_len;
    MPI_Get_processor_name(processor_name, &name_len);
    colony_size = MAX_ANTS / world_size;
    ant_colony = new Ant_t[colony_size];
    initAnt();

    auto start = std::chrono::system_clock::now();

    for (int iter = 0; iter < 10; iter++)
    {
        if (world_rank != 0)
        {

            struct Ant_t ants[colony_size];

            for (int ant = 0; ant < colony_size; ant++)
            {
                while (ant_colony[ant].path_index < MAX_NODES)
                {
                    ant_colony[ant].next_node = selectNextCity(ant);
                    ant_colony[ant].tabu[ant_colony[ant].next_node] = 1;
                    ant_colony[ant].path[ant_colony[ant].path_index] = ant_colony[ant].next_node;
                    ant_colony[ant].path_index = ant_colony[ant].path_index + 1;
                    ant_colony[ant].tour_length += (*dist)[ant_colony[ant].cur_node][ant_colony[ant].next_node];
                    ant_colony[ant].cur_node = ant_colony[ant].next_node;
                }
                ant_colony[ant].tour_length += (*dist)[ant_colony[ant].path[MAX_NODES - 1]][ant_colony[ant].path[0]];
                ants[ant].tour_length = ant_colony[ant].tour_length;
                ants[ant] = ant_colony[ant];
            }

            MPI_Send(&ants, colony_size, Ant_type, 0, 1, MPI_COMM_WORLD);
        }

        else if (world_rank == 0)
        {

            Ant_t container_buffer_r[world_size][colony_size];
            Ant_t *container_buff[world_size];
            for (int i = 1; i < world_size; i++)
            {
                MPI_Recv(&container_buffer_r[i - 1], colony_size, Ant_type, i, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
            for (int colony = 0; colony < world_size - 1; colony++)
            {
                updatePheromone(container_buffer_r[colony]);
                int bestSol, bestant;
                bestSol, bestant = bestSolution(container_buffer_r[colony]);
            }
        }

        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Bcast(&pheromone[0][0], MAX_NODES * MAX_NODES, MPI_FLOAT, 0, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        if (world_rank != 0)
        {
            restartAnts();
        }
    }
    if (world_rank == 0)
    {

        auto end = std::chrono::system_clock::now();
        std::chrono::duration<float> duration = end - start;

        cout << best << " " << duration.count() << endl;
    }
}

int main()
{

    std::chrono::duration<float> dur;
    float solution;

    graph = "lin105";
    dist = new EdgeMatrix();
    constructTSP(graph, cities, dist);

    MPI_Init(NULL, NULL);
    solveTSP();

    MPI_Finalize();
}
}
