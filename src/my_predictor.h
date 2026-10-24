// my_predictor.h
// This file contains a sample gshare_predictor class.
// It is a simple 32,768-entry gshare with a history length of 15.

class gshare_update : public branch_update {
public:
	unsigned int index;
};

class gshare_predictor : public branch_predictor {
public:
#define HISTORY_LENGTH	15
#define TABLE_BITS	15
	gshare_update u;
	branch_info bi;
	unsigned int history;
	unsigned char tab[1<<TABLE_BITS];

	gshare_predictor (void) : history(0) { 
		memset (tab, 0, sizeof (tab));
	}

	branch_update *predict (branch_info & b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) {
			u.index = 
				  (history << (TABLE_BITS - HISTORY_LENGTH)) 
				^ (b.address & ((1<<TABLE_BITS)-1));
			u.direction_prediction (tab[u.index] >> 1);
		} else {
			u.direction_prediction (true);
		}
		u.target_prediction (0);
		return &u;
	}

	void update (branch_update *u, bool taken, unsigned int target) {
		if (bi.br_flags & BR_CONDITIONAL) {
			unsigned char *c = &tab[((gshare_update*)u)->index];
			if (taken) {
				if (*c < 3) (*c)++;
			} else {
				if (*c > 0) (*c)--;
			}
			history <<= 1;
			history |= taken;
			history &= (1<<HISTORY_LENGTH)-1;
		}
	}
};

//
// Pentium M hybrid branch predictors
// This class implements a simple hybrid branch predictor based on the Pentium M branch outcome prediction units. 
// Instead of implementing the complete Pentium M branch outcome predictors, the class below implements a hybrid 
// predictor that combines a bimodal predictor and a global predictor. 
//
// The bimodal table is indexed by the low bits of the branch address.  The
// global predictor is a set-associative, tagged table of 2-bit counters
// indexed by (branch address XOR global history): the upper bits of that
// hash select the set, the lower bits are the tag (Index = HASH[14:6],
// Tag = HASH[5:0] in the assignment figure).  When some way of the selected
// set holds a matching tag the global prediction is used, otherwise the
// bimodal prediction is used and the LRU way of the set is allocated to the
// branch.  Only conditional branches read or train the tables; other
// branches are predicted taken.
//
// Table sizes, following the Pentium M figures in the assignment.
#define PM_BIMODAL_BITS		12	// bimodal table: 2^12 = 4096 2-bit counters
#define PM_HISTORY_LENGTH	15	// global history bits kept in the GHR
#define PM_GLOBAL_INDEX_BITS	9	// global predictor: 2^9 = 512 sets per way
#define PM_GLOBAL_TAG_BITS	6	// tag bits stored in each global entry
#define PM_GLOBAL_WAYS		4	// associativity of the global predictor

#define PM_HASH_BITS	(PM_GLOBAL_INDEX_BITS + PM_GLOBAL_TAG_BITS)
#define PM_BIMODAL_SIZE	(1 << PM_BIMODAL_BITS)
#define PM_GLOBAL_SETS	(1 << PM_GLOBAL_INDEX_BITS)

class pm_update : public branch_update {
public:
	unsigned int bindex;		// bimodal table index
	unsigned int gindex;		// global predictor set (row)
	unsigned int gtag;		// global predictor tag
	int way;			// way that hit, or the LRU way to allocate on a miss
	bool hit;			// some way of the set matched the tag
	bool bimodal_prediction;
	bool global_prediction;		// prediction of the counter in that way
};

// one entry of one way of the global predictor
struct pm_global_entry {
	bool valid;
	unsigned int tag;
	unsigned char counter;		// 2-bit saturating counter
	unsigned char age;		// LRU age: 0 = most recently used,
					// PM_GLOBAL_WAYS-1 = least recently used
};

class pm_predictor : public branch_predictor {
public:
	pm_update u;
	branch_info bi;
	unsigned int history;				// GHR, newest outcome in bit 0
	unsigned char bimodal[PM_BIMODAL_SIZE];		// 2-bit counters
	pm_global_entry global[PM_GLOBAL_WAYS][PM_GLOBAL_SETS];

	pm_predictor (void) : history(0) {
		memset (bimodal, 0, sizeof (bimodal));
		for (int w = 0; w < PM_GLOBAL_WAYS; w++) {
			for (int s = 0; s < PM_GLOBAL_SETS; s++) {
				global[w][s].valid = false;
				global[w][s].tag = 0;
				global[w][s].counter = 0;
				// way 0 starts as the LRU way, so an empty set
				// fills from way 0 upwards
				global[w][s].age = PM_GLOBAL_WAYS - 1 - w;
			}
		}
	}

	// saturating 2-bit counter update
	static void train (unsigned char *c, bool taken) {
		if (taken) {
			if (*c < 3) (*c)++;
		} else {
			if (*c > 0) (*c)--;
		}
	}

	branch_update *predict (branch_info & b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) {
			// bimodal prediction from the low address bits
			u.bindex = b.address & (PM_BIMODAL_SIZE - 1);
			u.bimodal_prediction = bimodal[u.bindex] >> 1;

			// global predictor lookup: hash the address with the
			// history, split the hash into set index and tag
			unsigned int hash = (b.address ^ history) & ((1 << PM_HASH_BITS) - 1);
			u.gindex = hash >> PM_GLOBAL_TAG_BITS;
			u.gtag = hash & ((1 << PM_GLOBAL_TAG_BITS) - 1);

			// search the ways of the set for the tag; remember the
			// LRU way in case nothing matches
			u.hit = false;
			u.way = 0;
			for (int w = 0; w < PM_GLOBAL_WAYS; w++) {
				pm_global_entry *e = &global[w][u.gindex];
				if (e->valid && e->tag == u.gtag) {
					u.hit = true;
					u.way = w;
					break;
				}
				if (e->age == PM_GLOBAL_WAYS - 1) u.way = w;
			}
			u.global_prediction = global[u.way][u.gindex].counter >> 1;

			// final prediction: global on a tag hit, bimodal otherwise
			u.direction_prediction (u.hit ? u.global_prediction : u.bimodal_prediction);
		} else {
			u.direction_prediction (true);
		}

		// branch target prediction is not part of this assignment
		u.target_prediction (0);
		return &u;
	}

	void update (branch_update *bu, bool taken, unsigned int target) {
		if (!(bi.br_flags & BR_CONDITIONAL)) return;
		pm_update *pu = (pm_update *) bu;

		// the bimodal counter is trained on every conditional branch
		train (&bimodal[pu->bindex], taken);

		// global predictor: train the matching entry, or allocate the
		// LRU way of the set to this branch.  As in Appendix B of the
		// assignment, a replaced entry keeps its counter and trains it.
		pm_global_entry *e = &global[pu->way][pu->gindex];
		if (!pu->hit) {
			e->valid = true;
			e->tag = pu->gtag;
		}
		train (&e->counter, taken);

		// the touched way becomes most recently used; ways that were
		// younger than it age by one
		for (int w = 0; w < PM_GLOBAL_WAYS; w++) {
			pm_global_entry *o = &global[w][pu->gindex];
			if (o->age < e->age) o->age++;
		}
		e->age = 0;

		// shift the outcome into the global history
		history <<= 1;
		history |= taken;
		history &= (1 << PM_HISTORY_LENGTH) - 1;
	}

};

//
// Complete Pentium M branch predictors for extra credit
// This class implements the complete Pentium M branch prediction units. 
// It implements both branch target prediction and branch outcome predicton. 
class cpm_update : public branch_update {
public:
        unsigned int index;
};

class cpm_predictor : public branch_predictor {
public:
        cpm_update u;

        cpm_predictor (void) {
        }

        branch_update *predict (branch_info & b) {
            u.direction_prediction (true);
            u.target_prediction (0);
            return &u;
        }

        void update (branch_update *u, bool taken, unsigned int target) {
        }

};


