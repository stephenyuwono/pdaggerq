<div align="center">
  <img src="./pq_graph.svg" width="400" alt="pq_graph Logo">
</div>

# PQ-Graph: Module for pdaggerq

pq_graph is an extension of the pdaggerq package, introducing graph-theoretical techniques to optimize many-body equations generated by pdaggerq. pq_graph will automatically generate intermediates and reorder expressions to minimize the number of floating point operations required to evaluate the equations. The module will generate code to evaluate these expressions using either Python with [Numpy](https://numpy.org/) or C++ with [TiledArray](https://valeevgroup.github.io/tiledarray/dox-master/index.html).

The expressions are stored in data structures that represent tensor contractions as [directed graphs](https://en.wikipedia.org/wiki/Quiver_(mathematics)), with tensors represented by vertices. The edges of the graph represent the indices of the tensors, and the contraction of the tensors is represented by the connection of the edge. This representation is analyzed to determine the optimal order of contraction and can generate the graph in a format that can be visualized using [Graphviz](https://graphviz.org/).

## Python API usage

#



```python
import pdaggerq

# set up pq_graph
graph = pdaggerq.pq_graph({
    "print_level": 2,  # verbosity level:
                       # 0: no printing of optimization steps (default)
                       # 1: print optimization steps without fusion or merging
                       # 2: print optimization steps with fusion and merging
    "permute_eri": True,  # whether to permute two-electron integrals to common order (default: true)
    "no_scalars": False,  # whether to skip the scalar terms in the final equations (default: false)
    "use_trial_index": True, # whether to store trial vectors as an additional index/dimension 
                             # for tensors in a sigma-vector build (default: false)
    "separate_sigma": True,  # whether to separate reusable intermediates for sigma-vector build (default: false)
    "opt_level": 6,  # optimization level:
                     # 0: no optimization
                     # 1: single-term optimization only (reordering)
                     # 2: reordering and subexpression elimination (substitution)
                     # 3: reordering, substitution, and separation of reusable intermediates (for sigma vectors)
                     # 4: reordering, substitution, and separation; unused intermediates are removed (pruning)
                     # 5: reordering, substitution, separation, pruning, and merging of equivalent terms
                     # 6: reordering, substitution, separation, pruning, merging, and fusion of intermediates (default)
    "batched": False,  # candidate substitutions are applied in batches rather than one at a time. (default: false)
    # Generally faster, but may not yield optimal results compared to single substitutions.
    "batch_size": 10,  # size of the batch for batched substitution (default: 10; -1 for no limit)
    "max_temps": -1,  # maximum number of intermediates to find (default: -1 for no limit)
    "max_depth": -1,  # maximum depth for chain of contractions (default: -1 for no limit)
    "low_memory": False,  # whether to recompute or save all permutations of each term in memory (default: false)
                          # if true, permutations are recomputed on the fly. Recommended if memory runs out.
    "nthreads": 12,  # number of threads to use (default: OMP_NUM_THREADS | available: 12)
})

T = ['t1', 't2'] # cluster amplitudes
left_ops = { # projection equations
    "singles_residual": [['e1(i,a)']],     # singles ( 0 = <0| i* a e(-T) H e(T) |0> )
    "doubles_residual": [['e2(i,j,b,a)']]  # doubles ( 0 = <0| i* j* b a e(-T) H e(T) |0> )
}

for eq_name, ops in left_ops.items():
    pq = pdaggerq.pq_helper('fermi')
    pq.set_left_operators(ops)
    pq.add_st_operator(1.0,['f'], T)
    pq.add_st_operator(1.0,['v'], T)
    pq.simplify()

    # queue up the equation for optimization:
    # 1) pass the pq_helper object and the name of the equation.
    # 2) the name is used to label the left-hand side (lhs) of the equation
    # 3) the last argument (optional) overrides the ordering of the external indices of the lhs
    graph.add(pq, eq_name, ['a', 'b', 'i', 'j'])
    pq.clear()

# optimize the equations
graph.optimize()       # reorders contraction and generates intermediates
graph.print("python")  # print the optimized equations for Python.
graph.analysis()       # prints the FLOP scaling (permutations are expanded into repeated terms for analysis)

# create a DOT file for use with Graphviz
graph.write_dot("ccsd.dot") 
```