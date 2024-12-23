//
// pdaggerq - A code for bringing strings of creation / annihilation operators to normal order.
// Filename: equation.cc
// Copyright (C) 2020 A. Eugene DePrince III
//
// Author: A. Eugene DePrince III <adeprince@fsu.edu>
// Maintainer: DePrince group
//
// This file is part of the pdaggerq package.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//

#include "../include/equation.h"

#ifdef _OPENMP
#include <omp.h>
#else
#define omp_get_max_threads() 1
#define omp_get_thread_num() 0
#define omp_get_num_threads() 1
#define omp_set_num_threads(n) 1
#endif
#include <iostream>

namespace pdaggerq {

    Equation::Equation(const string &name, const vector<vector<string>> &term_strings) {

        // set name of equation
        name_ = name;

        /// construct terms
        terms_.reserve(term_strings.size()); // reserve space for terms
        for (const auto & term_string : term_strings) {
            if (term_string.empty()) throw invalid_argument("Empty term string for Equation: " + name);
            terms_.emplace_back(name, term_string); // create terms
        }

        // set assignment vertex
        assignment_vertex_ = terms_.front().lhs();

        // set equation vertex for all terms
        for (auto & term : terms_)
            term.eq() = assignment_vertex_;

        // collect scaling of equations
        collect_scaling();

    }

    Equation::Equation(const string &name, const vector<Term> &terms) {

        // set name of equation
        name_ = name;

        if (terms.empty()) // throw error if no terms
            throw invalid_argument("Empty terms for Equation: " + name);

        // set assignment vertex
        assignment_vertex_ = terms.back().lhs();

        // set terms
        terms_ = terms;

        // set equation vertex for all terms if applicable
        for (auto &term: terms_)
            term.eq()  = assignment_vertex_;

        // collect scaling of equations
        collect_scaling();

    }

    Equation::Equation(const VertexPtr &assignment, const vector<Term> &terms) {

        // set name of equation
        name_ = assignment->name();

        // set assignment vertex
        assignment_vertex_ = assignment;

        // set terms
        terms_ = terms;

        // set equation vertex for all terms if applicable
        for (auto &term: terms_)
            term.eq()  = assignment_vertex_;

        // collect scaling of equations
        collect_scaling();

    }

    bool Equation::empty() {
        return terms_.empty();
    }

    void Equation::collect_scaling(bool regenerate) {

        if (terms_.empty()) return; // if no terms, return (nothing to do)

        // reset scaling maps
        flop_map_.clear(); // clear flop scaling map
        mem_map_.clear(); // clear memory scaling map

        for (auto & term : terms_) { // iterate over terms
            // compute scaling from term
            term.compute_scaling(regenerate);

            // collect scaling of terms
            scaling_map term_flop_map = term.flop_map(),
                        term_mem_map  = term.mem_map();

            flop_map_ += term_flop_map; // add flop scaling map
            mem_map_  += term_mem_map; // add memory scaling map
        }

        vertex_vector all_term_linkages;
        for (auto & term : terms_) {
            all_term_linkages.push_back(term.term_linkage());
        }

        // if there are no terms with linkages, return
        if (all_term_linkages.empty()) return;

    }

    void Equation::reorder(bool recompute) {
        // reorder rhs in term
        for (auto & term : terms_)
            term.reorder(recompute);

        // collect scaling of terms
        collect_scaling();
    }

    void Equation::insert(const Term& term, int index) {
        if (index < 0) index = (int)terms_.size() + index + 1; // convert negative index to positive index from end
        terms_.insert(terms_.begin() + index, term); // add term to index of terms
    }

    struct TermHash { // hash functor for finding similar terms
        size_t operator()(const Term& term) const {
            constexpr LinkageHash link_hasher;
            VertexPtr total_representation = term.lhs() + term.term_linkage();

            return link_hasher(as_link(total_representation));
        }
    };

    struct TermEqual { // predicate functor for finding similar terms
        bool operator()(const Term& term1, const Term& term2) const {
            // determine if permutation is the same
            if (term1.perm_type()  != term2.perm_type()) return false;
            if (term1.term_perms() != term2.term_perms()) return false;

            // compare term linkages
            LinkagePtr total_representation1 = as_link(term1.lhs() + term1.term_linkage(true));
            LinkagePtr total_representation2 = as_link(term2.lhs() + term2.term_linkage(true));

            return *total_representation1 == *total_representation2;
        }
    };

    size_t Equation::merge_terms() {
        if (is_temp_equation_) return 0; // don't merge temporary equations

        size_t terms_size = terms_.size(); // number of terms before merging
        vector<Term> new_terms; // new terms


        // iterate over the map, adding each term to the new_terms vector
        // map of terms to their associated coefficients
        std::unordered_map<Term, double, TermHash, TermEqual> term_count;
        for (auto &term : terms_) {
            string term_str = term.str(); // get term string
            // check if term is in map
            auto it = term_count.find(term);
            if (it != term_count.end()) {
                string unique_term_str = it->first.str(); // get unique term string
                // if term is in map, increment coefficient
                it->second += term.coefficient_;

                // add original pq to unique term
                if (Vertex::print_type_ == "python")
                    it->first.original_pq_ += "\n    # ";
                else if (Vertex::print_type_ == "c++")
                    it->first.original_pq_ += "\n    // ";

                it->first.original_pq_ += string(term.lhs()->name().size(), ' ');
                it->first.original_pq_ += " += " + term.original_pq_;
            } else {
                // if term is not in map, add term to map
                term_count[term] = term.coefficient_;
            }
        }


        for (auto &[unique_term, coeff] : term_count) {

            Term new_term = unique_term; // copy term
            new_term.coefficient_ = coeff; // set coefficient

            // skip terms with zero coefficients
            if (fabs(new_term.coefficient_) <= 1e-12)
                continue;

            // add term to new_terms
            new_terms.push_back(new_term);
        }


        terms_ = new_terms;
        collect_scaling(true);

        return terms_size - terms_.size();
    }

    set<Term *> Equation::get_temp_terms(const LinkagePtr& linkage) {
        // for every term, check if this linkage is within the term (do not check lhs)
        set<Term *> temp_terms;
        for (auto &term : terms_) {
            bool found = false;
            for (auto &op : term) {
                // check if this term has the temp
                found = op->has_temp(linkage);
                if (found) { temp_terms.insert(&term); break; }
            }

            // check if the lhs is the same temp if it was not found in the rhs
            if (!found && term.lhs()->same_temp(linkage))
                temp_terms.insert(&term);
        }

        // return the terms that contain the temp
        return temp_terms;
    }

    Equation Equation::clone() const {
        Equation copy = *this;
        if (assignment_vertex_) copy.assignment_vertex_ = assignment_vertex_->clone(); // deep copy assignment vertex

        // deep copy terms
        copy.terms_.clear();

        for (const auto &term : terms_) {
            copy.terms_.push_back(term.clone());
        }

        return copy;
    }

    void Equation::sort_tmp_type(vector<Term> &terms, const string &type) {

        // no terms, return
        if ( terms.empty() ) return;

        // to sort the tmps while keeping the order of terms without tmps, we need to
        // make a map of the equation terms and their index in the equation and sort that (so annoying)
        std::vector<pair<Term*, size_t>> indexed_terms;
        size_t eq_size = terms.size();
        indexed_terms.reserve(eq_size);
        for (size_t i = 0; i < eq_size; ++i)
            indexed_terms.emplace_back(&terms[i], i);

        // sort the terms by the maximum id of the tmps in the term, then by the index of the term
        bool consider_rhs = true;
        auto total_in_order = [type, &consider_rhs](const pair<Term*, size_t> &a, const pair<Term*, size_t> &b) {
            const auto &[a_term, a_idx] = a; const auto &[b_term, b_idx] = b;

            idset a_lhs_ids, b_lhs_ids;
            idset a_rhs_ids, b_rhs_ids;

            a_rhs_ids = a_term->term_linkage()->get_ids(type); b_rhs_ids = b_term->term_linkage()->get_ids(type);
            if (a_term->lhs()->is_temp()) a_lhs_ids = as_link(a_term->lhs())->get_ids(type);
            if (b_term->lhs()->is_temp()) b_lhs_ids = as_link(b_term->lhs())->get_ids(type);

            idset a_tot_ids, b_tot_ids;
            a_tot_ids.insert(a_lhs_ids.begin(), a_lhs_ids.end()); a_tot_ids.insert(a_rhs_ids.begin(), a_rhs_ids.end());
            b_tot_ids.insert(b_lhs_ids.begin(), b_lhs_ids.end()); b_tot_ids.insert(b_rhs_ids.begin(), b_rhs_ids.end());

            // get number of ids
            bool a_has_temp = !a_tot_ids.empty(), b_has_temp = !b_tot_ids.empty();

            // keep terms without temps first
            if (a_has_temp != b_has_temp) return !a_has_temp;

            // keep assignments first for the same total ids
            bool same_id = a_tot_ids == b_tot_ids;
            if ( same_id && a_term->is_assignment_ != b_term->is_assignment_)
                return a_term->is_assignment_;
            else if (same_id && !a_term->is_assignment_) return a_idx < b_idx;

            // if the max rhs id is larger than the max lhs id, then do not consider rhs
            long a_max_rhs = *std::max_element(a_rhs_ids.begin(), a_rhs_ids.end());
            long a_max_lhs = *std::max_element(a_lhs_ids.begin(), a_lhs_ids.end());
            long b_max_rhs = *std::max_element(b_rhs_ids.begin(), b_rhs_ids.end());
            long b_max_lhs = *std::max_element(b_lhs_ids.begin(), b_lhs_ids.end());
            long a_max = std::max(a_max_rhs, a_max_lhs);
            long b_max = std::max(b_max_rhs, b_max_lhs);

            bool a_rhs_greater = a_max_rhs > a_max_lhs;
            bool b_rhs_greater = b_max_rhs > b_max_lhs;

            if (!consider_rhs || a_rhs_greater || b_rhs_greater) {
                // keep in order of max ids
                return a_max < b_max;
            }

            // ensure that all ids in a_rhs are less than all ids in b_lhs
            for (long a_id : a_rhs_ids)
                for (long b_id : b_lhs_ids)
                    if (a_id >= b_id) return false;

            // ensure that all ids in b_rhs are greater than all ids in a_lhs
            for (long b_id : b_rhs_ids)
                for (long a_id : a_lhs_ids)
                    if (b_id < a_id) return false;

            // keep in lexicographical order
            return a_tot_ids < b_tot_ids;
        };

        consider_rhs = false;
        sort(indexed_terms.begin(), indexed_terms.end(), total_in_order);
        consider_rhs = true;
        stable_sort(indexed_terms.begin(), indexed_terms.end(), total_in_order);

        // initialize map of lhs names
        std::set<std::string> lhs_name_map;

        // replace the terms in the equation with the sorted terms
        std::vector<Term> sorted_terms;
        sorted_terms.reserve(indexed_terms.size());
        for (const auto &indexed_term : indexed_terms) {
            // check if lhs is in the map
            VertexPtr lhs = indexed_term.first->lhs();
            bool lhs_seen = lhs_name_map.find(lhs->name()) != lhs_name_map.end() && !lhs->is_temp();
            if (!lhs_seen) {
                lhs_name_map.insert(lhs->name());
                indexed_term.first->is_assignment_ = true;
            } else if (!lhs->is_temp()) {
                // lhs has already been used, so this is not an assignment
                indexed_term.first->is_assignment_ = false;
            } // else ignore temp assignments

            sorted_terms.push_back(*indexed_term.first);
        }

        terms = sorted_terms;
    }

    void Equation::rearrange(const string &type) {


        // sort by conditionals, then by permutation type, then by number of operators, then by cost.
        // This is a stable sort, so the order of terms with the same cost will be preserved.
        std::stable_sort(terms_.begin(), terms_.end(), [](const Term &a, const Term &b) {

            if (a.conditions() != b.conditions())
                return a.conditions() < b.conditions();

            // sort by name of lhs
            if (a.lhs()->name() != b.lhs()->name())
                return a.lhs()->name() < b.lhs()->name();

            // sort by number of operators
            if (a.size() != b.size())
                return a.size() < b.size();

            // sort by cost
            return a.flop_map() < b.flop_map();

        });

        // lastly, sort the terms by the maximum id of the tmps type in the term, then by the index of the term
        if (type == "all") { // rearrange by all temps in order
            sort_tmp_type(terms_, "scalar");
            sort_tmp_type(terms_, "reused");
            sort_tmp_type(terms_, "temp");
        } else sort_tmp_type(terms_, type); // else rearrange by the type of temp
    }

} // pdaggerq
