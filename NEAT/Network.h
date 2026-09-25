#ifndef _NEAT_NETWORK_H
#define _NEAT_NETWORK_H

#include <vector>

/**
 * One gene: a weighted connection between two neurons, identified by its
 * innovation number.
 */
struct Link {
    int innov;
    int inID, outID;
    double weight;
    bool enabled;

    Link (int _inID = -1, int _outID = -1, int _innov = -1,
          double _w = 0, bool _enabled = false) :
          innov(_innov), inID(_inID), outID(_outID), weight(_w),
          enabled(_enabled) {}

    void copy (const Link &l, const double enableRate);
    void copy (const Link &l1, const Link &l2, const double enableRate);

    void printLink() const;

    //For sorting links, go by innovation number
    bool operator< (const Link &l2) const {
        return innov < l2.innov;
    }
};

/**
 * Neural network built from a genome's links.  It keeps its own compact
 * copy of the structure (neuron indices, weights), so building and running
 * a network never touches the genome: several networks can be built from
 * one genome and run on different threads.
 *
 * Each run() makes one synchronous propagation step: every neuron's new
 * activation is the sigmoid of its weighted inputs from the previous step.
 * The first run() repeats steps until every neuron has received input (so
 * outputs are meaningful immediately), capped so an unreachable neuron
 * can't stall it.
 */
class Network {
    public:
        Network (int nInput, int nOutput, const Link *genome, int geneLength);

        void run (const double input[], double output[]);

        int numNeurons () const { return (int)neurons.size(); }
        int numConnections () const { return (int)conns.size(); }

        //Standard sigmoid, a little stretched
        static double sigmoid (double x);

    private:
        struct Connection {
            int from, to;       //Neuron indices
            double weight;
        };
        struct Neuron {
            double inputSum = 0;
            double activation = 0;
            bool active = false;        //Has received input
        };

        std::vector<Connection> conns;  //Enabled links, in genome order
        std::vector<Neuron> neurons;    //Inputs, outputs, then hidden
        int nInput, nOutput;
        bool initialized = false;

        bool allActive () const;
        void propagate (bool settling);
};

#endif
