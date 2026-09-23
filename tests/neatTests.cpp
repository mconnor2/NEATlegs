// Unit tests for the NEAT library (no physics or graphics)
#include <cmath>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "check.h"

#include "GeneticAlgorithm.h"
#include "Genome.h"
#include "InnovationStore.h"
#include "Network.h"
#include "random.h"

using namespace std;

namespace {

struct LinkRec {
    int innov, in, out;
    double weight;
    bool enabled;
};

string saved (const GenomeP &g) {
    ostringstream s;
    g->save(s);
    return s.str();
}

vector<LinkRec> linksOf (const GenomeP &g) {
    vector<LinkRec> links;
    istringstream in(saved(g));
    string line;
    while (getline(in, line)) {
	if (line.compare(0, 5, "link ") != 0) continue;
	istringstream ls(line.substr(5));
	LinkRec l;
	int enabled;
	ls>>l.innov>>l.in>>l.out>>l.weight>>enabled;
	l.enabled = enabled != 0;
	links.push_back(l);
    }
    return links;
}

GenomeP loadText (const string &text, ExpParameters *P) {
    istringstream in(text);
    return Genome::load(in, P);
}

ExpParameters params (int nIn, int nOut) {
    ExpParameters P;
    P.popSize = 50;
    P.nInput = nIn;
    P.nOutput = nOut;
    return P;
}

vector<double> runNet (const GenomeP &g, const vector<double> &input,
		       int nOut, int times = 1) {
    unique_ptr<Network> N(g->createNewNetwork());
    vector<double> in(input), out(nOut, 0.0);
    for (int i = 0; i < times; ++i) N->run(in.data(), out.data());
    return out;
}

double sigmoid (double x) { return 1.0/(1.0 + exp(-3.0*x)); }

// A genome with some structure: a few generations of mutation and
// structural growth, as the GA would produce
GenomeP grownGenome (ExpParameters *P, InnovationStore &IS, int gens = 20) {
    GenomeP g(new Genome(P));
    for (int i = 0; i < gens; ++i) {
	IS.newGeneration();
	g = g->singleMate(&IS);
	g->mutate();
    }
    return g;
}

}

TEST(saveLoadRoundTrip) {
    seed_rand(1);
    ExpParameters P = params(4, 2);
    P.addNodeMutationRate = 0.5;
    P.addLinkMutationRate = 0.5;
    InnovationStore IS(&P);
    GenomeP g = grownGenome(&P, IS);
    CHECK(g->numHiddenNodes() > 0);

    GenomeP back = loadText(saved(g), &P);
    CHECK(back != nullptr);
    if (!back) return;
    CHECK(saved(back) == saved(g));
    CHECK(back->numLinks() == g->numLinks());
    CHECK(back->numHiddenNodes() == g->numHiddenNodes());

    vector<double> in = {1.0, 0.3, -0.2, 0.7};
    vector<double> a = runNet(g, in, 2, 5), b = runNet(back, in, 2, 5);
    CHECK(a == b);
}

TEST(loadRejectsMismatchedExperiment) {
    ExpParameters P = params(2, 1), Q = params(3, 1);
    GenomeP g(new Genome(&P));
    CHECK(loadText(saved(g), &Q) == nullptr);
    CHECK(loadText("not a genome\n", &P) == nullptr);
    CHECK(loadText("genome inputs 2 outputs 1 nodes 3 links 2\n"
		   "link 0 0 2 0.5 1\n", &P) == nullptr);	// truncated
}

TEST(cloneIsEqualAndIndependent) {
    seed_rand(2);
    ExpParameters P = params(3, 2);
    P.weightMutationRate = 1.0;
    InnovationStore IS(&P);
    GenomeP g = grownGenome(&P, IS, 5);
    GenomeP c = g->clone();
    CHECK(saved(c) == saved(g));
    string before = saved(g);
    c->mutate();
    CHECK(saved(g) == before);		// original untouched
    CHECK(saved(c) != before);
}

TEST(compatProperties) {
    seed_rand(3);
    ExpParameters P = params(3, 2);
    InnovationStore IS(&P);
    GenomeP a = grownGenome(&P, IS, 10), b = grownGenome(&P, IS, 10);
    CHECK(a->compat(a) == 0.0);
    CHECK(a->compat(a->clone()) == 0.0);
    CHECK(a->compat(b) == b->compat(a));
    CHECK(a->compat(b) > 0.0);

    // Pure weight changes: distance is compatWDiff * total |dw|
    GenomeP c = a->clone();
    P.weightMutationRate = 1.0;
    c->mutate();
    vector<LinkRec> la = linksOf(a), lc = linksOf(c);
    double dw = 0;
    for (size_t i = 0; i < la.size(); ++i) dw += fabs(la[i].weight - lc[i].weight);
    CHECK_NEAR(a->compat(c), P.compatWDiff * dw, 1e-9);
}

TEST(matingKeepsInnovationsSortedAndUnique) {
    seed_rand(4);
    ExpParameters P = params(4, 2);
    P.addNodeMutationRate = 0.3;
    P.addLinkMutationRate = 0.3;
    InnovationStore IS(&P);
    vector<GenomeP> pop;
    for (int i = 0; i < 20; ++i) pop.push_back(GenomeP(new Genome(&P)));

    for (int gen = 0; gen < 30; ++gen) {
	IS.newGeneration();		// as GeneticAlgorithm does
	vector<GenomeP> next;
	for (int i = 0; i < 20; ++i) {
	    GenomeP p1 = pop[rand_int() % pop.size()], p2 = pop[rand_int() % pop.size()];
	    GenomeP child = p1->mate(p2, &IS);
	    child->mutate();

	    vector<LinkRec> lc = linksOf(child), l1 = linksOf(p1);
	    bool sorted = true;
	    for (size_t k = 1; k < lc.size(); ++k)
		if (lc[k].innov <= lc[k-1].innov) sorted = false;
	    CHECK(sorted);

	    // Without inheritAllLinks the child has every gene of the
	    // dominant parent (p1), plus at most 3 new ones
	    size_t k = 0;
	    for (const LinkRec &l : l1) {
		while (k < lc.size() && lc[k].innov < l.innov) ++k;
		CHECK(k < lc.size() && lc[k].innov == l.innov);
	    }
	    CHECK(lc.size() >= l1.size() && lc.size() <= l1.size() + 3);
	    next.push_back(child);
	}
	pop.swap(next);
    }
}

TEST(networkMatchesHandCalculation) {
    ExpParameters P = params(2, 1);
    // bias(0) and x(1) -> output(2); a disabled link must be ignored
    GenomeP g = loadText("genome inputs 2 outputs 1 nodes 3 links 3\n"
			 "link 0 0 2 0.2 1\n"
			 "link 1 1 2 0.4 1\n"
			 "link 2 1 2 50 0\n", &P);
    CHECK(g != nullptr);
    if (!g) return;
    double x = 0.5;
    vector<double> out = runNet(g, {1.0, x}, 1);
    CHECK_NEAR(out[0], sigmoid(0.2 + 0.4*x), 1e-12);
}

TEST(networkHiddenNodeSettles) {
    ExpParameters P = params(2, 1);
    // x -> hidden(3) -> output(2), plus bias -> output.  One propagation
    // step per run(), so with constant input it settles after two runs.
    GenomeP g = loadText("genome inputs 2 outputs 1 nodes 4 links 3\n"
			 "link 0 0 2 0.5 1\n"
			 "link 1 1 3 1.0 1\n"
			 "link 2 3 2 2.0 1\n", &P);
    CHECK(g != nullptr);
    if (!g) return;
    double x = -0.3;
    vector<double> out = runNet(g, {1.0, x}, 1, 3);
    CHECK_NEAR(out[0], sigmoid(0.5 + 2.0*sigmoid(x)), 1e-12);
}

// Networks keep their own copy of the structure: building and running
// them must not change the genome
TEST(networkLeavesGenomeUntouched) {
    seed_rand(5);
    ExpParameters P = params(3, 2);
    P.addNodeMutationRate = 0.5;
    InnovationStore IS(&P);
    GenomeP g = grownGenome(&P, IS, 10);
    string before = saved(g);
    runNet(g, {1.0, 0.2, 0.4}, 2, 10);
    CHECK(saved(g) == before);
}

// ...so one genome can be evaluated on several threads at once
TEST(sameGenomeOnManyThreads) {
    seed_rand(6);
    ExpParameters P = params(3, 2);
    P.addNodeMutationRate = 0.5;
    P.addLinkMutationRate = 0.5;
    InnovationStore IS(&P);
    GenomeP g = grownGenome(&P, IS, 15);
    vector<double> expected = runNet(g, {1.0, 0.3, -0.6}, 2, 50);

    const int T = 8;
    vector<vector<double>> got(T);
    vector<thread> threads;
    for (int t = 0; t < T; ++t)
	threads.emplace_back([&, t]() {
	    for (int rep = 0; rep < 20; ++rep)
		got[t] = runNet(g, {1.0, 0.3, -0.6}, 2, 50);
	});
    for (auto &th : threads) th.join();
    for (int t = 0; t < T; ++t) CHECK(got[t] == expected);
}

// A neuron nothing feeds (its only incoming link is disabled) used to make
// the first run() loop forever waiting for it to activate
TEST(unreachableNeuronDoesNotHang) {
    ExpParameters P = params(2, 1);
    GenomeP g = loadText("genome inputs 2 outputs 1 nodes 4 links 3\n"
			 "link 0 0 2 0.5 1\n"
			 "link 1 1 3 1.0 0\n"		// only way into neuron 3
			 "link 2 3 2 2.0 1\n", &P);
    CHECK(g != nullptr);
    if (!g) return;
    vector<double> out = runNet(g, {1.0, 0.5}, 1, 3);
    // neuron 3 sits at sigmoid(0) = 0.5 once it's updated
    CHECK_NEAR(out[0], sigmoid(0.5 + 2.0*0.5), 1e-12);
}

TEST(innovationsSharedWithinGeneration) {
    ExpParameters P = params(3, 2);
    InnovationStore IS(&P);
    int a, b, c;
    CHECK(!IS.addLink(0, 4, a));
    CHECK(IS.addLink(0, 4, b));		// seen this generation
    CHECK(a == b);
    CHECK(!IS.addLink(1, 4, c));
    CHECK(c != a);

    int pre1, post1, n1, pre2, post2, n2;
    CHECK(!IS.addNode(2, pre1, post1, n1));
    CHECK(IS.addNode(2, pre2, post2, n2));
    CHECK(pre1 == pre2 && post1 == post2 && n1 == n2);
    CHECK(n1 == P.nInput + P.nOutput);		// first new neuron id

    IS.newGeneration();
    int d;
    CHECK(!IS.addLink(0, 4, d));		// forgotten: new innovation
    CHECK(d != a);
}

TEST(seededRandomIsRepeatable) {
    seed_rand(42);
    vector<double> a;
    for (int i = 0; i < 100; ++i) a.push_back(i % 2 ? rand_double() : rand_gauss());
    seed_rand(42);
    vector<double> b;
    for (int i = 0; i < 100; ++i) b.push_back(i % 2 ? rand_double() : rand_gauss());
    CHECK(a == b);
    seed_rand(43);
    CHECK(rand_double() != a[1]);
}

namespace {

// XOR on fixed inputs: deterministic and cheap
double xorFitness (const GenomeP &g) {
    static const double cases[4][3] = {{0,0,0}, {0,1,1}, {1,0,1}, {1,1,0}};
    unique_ptr<Network> N(g->createNewNetwork());
    double err = 0;
    for (auto &c : cases) {
	double in[3] = {1.0, c[0], c[1]}, out[1] = {0};
	for (int k = 0; k < 4; ++k) N->run(in, out);
	err += (out[0] - c[2]) * (out[0] - c[2]);
    }
    return (g->fitness = 4.0 - err);
}

struct RunResult {
    vector<GenerationStats> history;
    string best;
};

RunResult runGA (uint64_t seed, int gens) {
    seed_rand(seed);
    ExpParameters P = params(3, 1);
    P.popSize = 60;
    P.compatThresh = 3.0;
    P.targetSpecies = 5;
    FitnessFunction f = xorFitness;
    GeneticAlgorithm GA(&P, &f);
    for (int i = 0; i < gens; ++i) GA.nextGeneration();
    return {GA.history(), saved(GA.topGenomes().at(0).second)};
}

bool sameStats (const GenerationStats &a, const GenerationStats &b) {
    return a.generation == b.generation && a.populationSize == b.populationSize &&
	   a.maxFitness == b.maxFitness && a.meanFitness == b.meanFitness &&
	   a.minFitness == b.minFitness && a.nSpecies == b.nSpecies &&
	   a.meanCompat == b.meanCompat && a.meanEnabledLinks == b.meanEnabledLinks;
}

}

// Same seed, same run, even though fitness is evaluated on many threads
TEST(gaIsDeterministicForASeed) {
    RunResult a = runGA(7, 25), b = runGA(7, 25);
    CHECK(a.history.size() == 25u && b.history.size() == 25u);
    bool same = a.history.size() == b.history.size();
    for (size_t i = 0; same && i < a.history.size(); ++i)
	same = sameStats(a.history[i], b.history[i]);
    CHECK(same);
    CHECK(a.best == b.best);

    RunResult c = runGA(8, 25);
    CHECK(c.best != a.best);
}

TEST(gaStatsAreConsistent) {
    RunResult r = runGA(9, 15);
    for (size_t i = 0; i < r.history.size(); ++i) {
	const GenerationStats &s = r.history[i];
	CHECK(s.generation == (int)i);
	CHECK(s.minFitness <= s.meanFitness && s.meanFitness <= s.maxFitness);
	CHECK(s.nSpecies >= 1 && s.nSpecies <= s.populationSize);
	int members = 0;
	for (const SpecieStats &sp : s.species) members += sp.size;
	CHECK(members == s.populationSize);
    }
}

int main () {
    return check::runTests();
}
