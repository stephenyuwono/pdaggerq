//
// pdaggerq - A code for bringing strings of creation / annihilation operators to normal order.
// Filename: pq_string.cc
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

#include "pq_string.h"
#include "pq_tensor.h"
#include "pq_utils.h"

#include<vector>
#include<iostream>
#include<string>
#include<algorithm>
#include<cmath>
#include<sstream>
#include <fstream>
#include<numeric>

namespace pdaggerq {

// constructor
pq_string::pq_string(const std::string &vacuum_type){

    vacuum = vacuum_type;
}

// sort amplitude, integral,and delta function labels and define key
void pq_string::sort() {

    // define numerical labels and permutations

    for (auto &ints_pair : ints) {
        std::vector<integrals> &ints_vec = ints_pair.second;

        // sort labels in integrals
        for (integrals & integral : ints_vec) {
            integral.sort();
        }

        // now, sort list of amplitudes based on labels
        size_t dim = ints_vec.size();

        std::vector<std::pair<std::vector<int>, int>> numerical_labels_with_index;
        for (size_t i = 0; i < dim; i++) {
            numerical_labels_with_index.emplace_back(ints_vec[i].numerical_labels, i);
        }

        // sort the list of pairs lexicographically by the numerical labels
        std::sort(numerical_labels_with_index.begin(), numerical_labels_with_index.end(),
                  [](const std::pair<std::vector<int>, int> &a,
                     const std::pair<std::vector<int>, int> &b) {
                      return a.first < b.first;
                  });

        std::vector<integrals> tmp;
        for (const auto& pair : numerical_labels_with_index) {
            tmp.push_back(ints_vec[pair.second]);
        }
        ints_vec = tmp;
    }

    for (auto &amps_pair : amps) {
        std::vector<amplitudes> &amps_vec = amps_pair.second;

        // sort labels in amplitudes
        for (amplitudes & amp : amps_vec) {
            amp.sort();
        }

        // now, sort list of amplitudes based on labels
        size_t dim = amps_vec.size();
        std::vector<std::pair<std::vector<int>, int>> numerical_labels_with_index;
        for (size_t i = 0; i < dim; i++) {
            numerical_labels_with_index.emplace_back(amps_vec[i].numerical_labels, i);
        }
        // sort the list of pairs lexicographically by the numerical labels
        std::sort(numerical_labels_with_index.begin(), numerical_labels_with_index.end(),
                  [](const std::pair<std::vector<int>, int> &a,
                     const std::pair<std::vector<int>, int> &b) {
                      return a.first < b.first;
                  });

        std::vector<amplitudes> tmp;
        for (const auto& pair : numerical_labels_with_index) {
            tmp.push_back(amps_vec[pair.second]);
        }
        amps_vec = tmp;
    }

    // sort labels in deltas
    for (delta_functions & delta : deltas) {
        delta.sort();
    }

    // now, sort list of deltas based on labels
    size_t dim = deltas.size();
    if ( dim > 0 ) {

        std::vector<std::pair<std::vector<int>, int>> numerical_labels_with_index;
        for (size_t i = 0; i < dim; i++) {
            numerical_labels_with_index.emplace_back(deltas[i].numerical_labels, i);
        }

        // sort the list of pairs lexicographically by the numerical labels
        std::sort(numerical_labels_with_index.begin(), numerical_labels_with_index.end(),
                  [](const std::pair<std::vector<int>, int> &a,
                     const std::pair<std::vector<int>, int> &b) {
                      return a.first < b.first;
                  });

        std::vector<delta_functions> tmp;
        for (const auto& pair : numerical_labels_with_index) {
            tmp.push_back(deltas[pair.second]);
        }
        deltas = tmp;
    }

    key = get_key();
}

// is string in normal order? both fermion and boson parts
bool pq_string::is_normal_order() {

    // don't bother bringing to normal order if we're going to skip this string
    if (skip) return true;

    // fermions
    if ( vacuum == "TRUE" ) {
        for (int i = 0; i < (int)symbol.size()-1; i++) {
            if ( !is_dagger[i] && is_dagger[i+1] ) {
                return false;
            }
        }
    }else {
        for (int i = 0; i < (int)symbol.size()-1; i++) {
            // check if stings should be zero or not
            bool is_dagger_right = is_dagger_fermi[symbol.size()-1];
            bool is_dagger_left  = is_dagger_fermi[0];
            if ( !is_dagger_right || is_dagger_left ) {
                skip = true; // added 5/28/21
                return true;
            }
            if ( !is_dagger_fermi[i] && is_dagger_fermi[i+1] ) {
                return false;
            }
        }
    }

    // bosons
    if ( !is_boson_normal_order() ) {
        return false;
    }

    return true;
}

// is boson part of string in normal order?
bool pq_string::is_boson_normal_order() {

    if ( is_boson_dagger.size() == 1 ) {
        bool is_dagger_right = is_boson_dagger[0];
        bool is_dagger_left  = is_boson_dagger[0];
        if ( !is_dagger_right || is_dagger_left ) {
            skip = true;
            return true;
        }
    }
    for (int i = 0; i < (int)is_boson_dagger.size()-1; i++) {

        // check if stings should be zero or not ... added 5/28/21
        bool is_dagger_right = is_boson_dagger[is_boson_dagger.size()-1];
        bool is_dagger_left  = is_boson_dagger[0];
        if ( !is_dagger_right || is_dagger_left ) {
            skip = true;
            return true;
        }

        if ( !is_boson_dagger[i] && is_boson_dagger[i+1] ) {
            return false;
        }
    }
    return true;
}

// print string information
void pq_string::print() const {

    if ( skip ) return;

    if ( vacuum == "FERMI" && !symbol.empty() ) {
        // check if stings should be zero or not
        bool is_dagger_right = is_dagger_fermi[symbol.size()-1];
        bool is_dagger_left  = is_dagger_fermi[0];
        if ( !is_dagger_right || is_dagger_left ) {
            //return;
        }
    }

    double fac = fabs(factor);
    int sgn = factor < 0.0 ? -sign : sign;

    printf("    ");
    printf("//     ");
    printf("%c", sgn > 0 ? '+' : '-');
    printf(" ");

    int precision = minimum_precision(fac);
    std::string factor_str = to_string_with_precision(fabs(fac), precision);
    printf("%s", factor_str.c_str());
    printf(" ");

    if ( !permutations.empty() ) {
        // should have an even number of symbols...how many pairs?
        size_t n = permutations.size() / 2;
        int count = 0;
        for (int i = 0; i < n; i++) {
            printf("P(");
            printf("%s", permutations[count++].c_str());
            printf(",");
            printf("%s", permutations[count++].c_str());
            printf(")");
            printf(" ");
        }
    }
    if ( !paired_permutations_2.empty() ) {
        // should have an number of symbols divisible by 4
        size_t n = paired_permutations_2.size() / 4;
        int count = 0;
        for (int i = 0; i < n; i++) {
            printf("PP2(");
            printf("%s",paired_permutations_2[count++].c_str());
            printf(",");
            printf("%s",paired_permutations_2[count++].c_str());
            printf(",");
            printf("%s",paired_permutations_2[count++].c_str());
            printf(",");
            printf("%s",paired_permutations_2[count++].c_str());
            printf(")");
            printf(" ");
        }
    }
    if ( !paired_permutations_6.empty() ) {
        // should have an number of symbols divisible by 6
        size_t n = paired_permutations_6.size() / 6;
        int count = 0;
        for (int i = 0; i < n; i++) {
            printf("PP6(");
            printf("%s",paired_permutations_6[count++].c_str());
            printf(",");
            printf("%s",paired_permutations_6[count++].c_str());
            printf(",");
            printf("%s",paired_permutations_6[count++].c_str());
            printf(",");
            printf("%s",paired_permutations_6[count++].c_str());
            printf(",");
            printf("%s",paired_permutations_6[count++].c_str());
            printf(",");
            printf("%s",paired_permutations_6[count++].c_str());
            printf(")");
            printf(" ");
        }
    }
    if ( !paired_permutations_3.empty() ) {
        // should have an number of symbols divisible by 6
        size_t n = paired_permutations_3.size() / 6;
        int count = 0;
        for (int i = 0; i < n; i++) {
            printf("PP3(");
            printf("%s",paired_permutations_3[count++].c_str());
            printf(",");
            printf("%s",paired_permutations_3[count++].c_str());
            printf(",");
            printf("%s",paired_permutations_3[count++].c_str());
            printf(",");
            printf("%s",paired_permutations_3[count++].c_str());
            printf(",");
            printf("%s",paired_permutations_3[count++].c_str());
            printf(",");
            printf("%s",paired_permutations_3[count++].c_str());
            printf(")");
            printf(" ");
        }
    }

    for (size_t i = 0; i < symbol.size(); i++) {
        printf("%s", symbol[i].c_str());
        if ( is_dagger[i] ) {
            printf("%c", '*');
        }
        printf(" ");
    }

    // print deltas
    for (const delta_functions & delta : deltas) {
        delta.print();
    }

    // print integrals
    for (const auto &[type, ints_vec] : ints) {
        for (const integrals & integral : ints_vec) {
            integral.print(type);
        }
    }

    // print amplitudes
    for (const auto &[type, amps_vec] : amps) {
        for (const amplitudes & amp : amps_vec) {
            amp.print(type);
        }
    }

    // bosons:
    for (auto && is_bdag : is_boson_dagger) {
        if ( is_bdag ) {
            printf("B* ");
        }else {
            printf("B ");
        }
    }
    if ( has_w0 ) {
        printf("w0");
        printf(" ");
    }

    printf("\n");
}

// return identifier for pq_string as std::string
std::string pq_string::get_key() {

    if (is_spin_blocked || is_range_blocked) {
        //printf("\n");
        //printf("    >>> WARNING <<<\n");
        //printf("\n");
        //printf("    pq_string::get_key() is not meant to work with spin or range blocking\n");
        //printf("\n");
    }

    std::string my_string = "";

    std::string tmp;

    if ( skip ) return my_string;

    if ( !permutations.empty() ) {
        // should have an even number of symbols...how many pairs?
        size_t n = permutations.size() / 2;
        size_t count = 0;
        for (size_t i = 0; i < n; i++) {
            tmp  = "P(";
            tmp += permutations[count++];
            tmp += ",";
            tmp += permutations[count++];
            tmp += ")";
            my_string += tmp;
        }
    }

    if ( !paired_permutations_2.empty() ) {
        // should have a number of symbols divisible by 4
        size_t n = paired_permutations_2.size() / 4;
        size_t count = 0;
        for (size_t i = 0; i < n; i++) {
            tmp  = "PP2(";
            tmp += paired_permutations_2[count++];
            tmp += ",";
            tmp += paired_permutations_2[count++];
            tmp += ",";
            tmp += paired_permutations_2[count++];
            tmp += ",";
            tmp += paired_permutations_2[count++];
            tmp += ")";
            my_string += tmp;
        }
    }

    if ( !paired_permutations_6.empty() ) {
        // should have a number of symbols divisible by 6
        size_t n = paired_permutations_6.size() / 6;
        size_t count = 0;
        for (size_t i = 0; i < n; i++) {
            tmp  = "PP6(";
            tmp += paired_permutations_6[count++];
            tmp += ",";
            tmp += paired_permutations_6[count++];
            tmp += ",";
            tmp += paired_permutations_6[count++];
            tmp += ",";
            tmp += paired_permutations_6[count++];
            tmp += ",";
            tmp += paired_permutations_6[count++];
            tmp += ",";
            tmp += paired_permutations_6[count++];
            tmp += ")";
            my_string += tmp;
        }
    }

    if ( !paired_permutations_3.empty() ) {
        // should have a number of symbols divisible by 6
        size_t n = paired_permutations_3.size() / 6;
        size_t count = 0;
        for (size_t i = 0; i < n; i++) {
            tmp  = "PP3(";
            tmp += paired_permutations_3[count++];
            tmp += ",";
            tmp += paired_permutations_3[count++];
            tmp += ",";
            tmp += paired_permutations_3[count++];
            tmp += ",";
            tmp += paired_permutations_3[count++];
            tmp += ",";
            tmp += paired_permutations_3[count++];
            tmp += ",";
            tmp += paired_permutations_3[count++];
            tmp += ")";
            my_string += tmp;
        }
    }

    // creation / annihilation operators
    for (size_t i = 0; i < symbol.size(); i++) {

        std::string tmp_symbol = symbol[i];
        if ( is_dagger[i] ) {
            tmp_symbol += "*";
        }
        my_string += tmp_symbol;
    }

    // deltas
    for (const delta_functions & delta : deltas) {
        std::vector<std::string> numerical_labels_string(delta.numerical_labels.size());
        std::transform(delta.numerical_labels.begin(), delta.numerical_labels.end(), numerical_labels_string.begin(),
                       [](int num) { return std::to_string(num); });
        my_string += "delta";
        my_string += std::accumulate(numerical_labels_string.begin(), numerical_labels_string.end(), std::string());
    }

    // integrals
    for (auto & type : integral_types) {
        if (ints.find(type) == ints.end()) continue;
        std::vector<integrals> &ints_vec = ints[type];
        for (integrals & integral : ints_vec) {
            std::vector<std::string> numerical_labels_string(integral.numerical_labels.size());
            std::transform(integral.numerical_labels.begin(), integral.numerical_labels.end(), numerical_labels_string.begin(),
                           [](int num) { return std::to_string(num); });
            my_string += type;
            my_string += std::accumulate(numerical_labels_string.begin(), numerical_labels_string.end(), std::string());
        }
    }

    // amplitudes
    for (auto & type : amplitude_types) {
        if (amps.find(type) == amps.end()) continue;
        std::vector<amplitudes> &amps_vec = amps[type];
        for (amplitudes & amp : amps_vec) {
            std::vector<std::string> numerical_labels_string(amp.numerical_labels.size());
            std::transform(amp.numerical_labels.begin(), amp.numerical_labels.end(), numerical_labels_string.begin(),
                           [](int num) { return std::to_string(num); });
            my_string += type;
            my_string += std::accumulate(numerical_labels_string.begin(), numerical_labels_string.end(), std::string());
        }
    }

    // bosons:
    for (auto && is_bdag : is_boson_dagger) {
        if ( is_bdag ) {
            my_string += "B*";
        }else {
            my_string += "B";
        }
    }
    if ( has_w0 ) {
        my_string += "w0";
    }

    return my_string;
}

// return string information
std::vector<std::string> pq_string::get_string() {

    std::vector<std::string> my_string;

    if ( skip ) return my_string;

    if ( vacuum == "FERMI" && !symbol.empty() ) {
        // check if stings should be zero or not
        bool is_dagger_right = is_dagger_fermi[symbol.size()-1];
        bool is_dagger_left  = is_dagger_fermi[0];
        if ( !is_dagger_right || is_dagger_left ) {
            //return;
        }
    }

    if ( factor < 0.0 ) {
        factor = fabs(factor);
        sign *= -1;
    }
    std::string tmp;
    if ( sign > 0 ) {
        tmp = "+";
    }else {
        tmp = "-";
    }

    int precision = minimum_precision(factor);
    my_string.push_back(tmp + to_string_with_precision(fabs(factor), precision));

    if ( !permutations.empty() ) {
        // should have an even number of symbols...how many pairs?
        size_t n = permutations.size() / 2;
        size_t count = 0;
        for (size_t i = 0; i < n; i++) {
            tmp  = "P(";
            tmp += permutations[count++];
            tmp += ",";
            tmp += permutations[count++];
            tmp += ")";
            my_string.push_back(tmp);
        }
    }

    if ( !paired_permutations_2.empty() ) {
        // should have a number of symbols divisible by 4
        size_t n = paired_permutations_2.size() / 4;
        size_t count = 0;
        for (size_t i = 0; i < n; i++) {
            tmp  = "PP2(";
            tmp += paired_permutations_2[count++];
            tmp += ",";
            tmp += paired_permutations_2[count++];
            tmp += ",";
            tmp += paired_permutations_2[count++];
            tmp += ",";
            tmp += paired_permutations_2[count++];
            tmp += ")";
            my_string.push_back(tmp);
        }
    }

    if ( !paired_permutations_6.empty() ) {
        // should have a number of symbols divisible by 6
        size_t n = paired_permutations_6.size() / 6;
        size_t count = 0;
        for (size_t i = 0; i < n; i++) {
            tmp  = "PP6(";
            tmp += paired_permutations_6[count++];
            tmp += ",";
            tmp += paired_permutations_6[count++];
            tmp += ",";
            tmp += paired_permutations_6[count++];
            tmp += ",";
            tmp += paired_permutations_6[count++];
            tmp += ",";
            tmp += paired_permutations_6[count++];
            tmp += ",";
            tmp += paired_permutations_6[count++];
            tmp += ")";
            my_string.push_back(tmp);
        }
    }

    if ( !paired_permutations_3.empty() ) {
        // should have a number of symbols divisible by 6
        size_t n = paired_permutations_3.size() / 6;
        size_t count = 0;
        for (size_t i = 0; i < n; i++) {
            tmp  = "PP3(";
            tmp += paired_permutations_3[count++];
            tmp += ",";
            tmp += paired_permutations_3[count++];
            tmp += ",";
            tmp += paired_permutations_3[count++];
            tmp += ",";
            tmp += paired_permutations_3[count++];
            tmp += ",";
            tmp += paired_permutations_3[count++];
            tmp += ",";
            tmp += paired_permutations_3[count++];
            tmp += ")";
            my_string.push_back(tmp);
        }
    }

    // creation / annihilation operators
    for (size_t i = 0; i < symbol.size(); i++) {

        std::string tmp_symbol = "a";
        if ( is_dagger[i] ) {
            tmp_symbol += "*";
        }
        tmp_symbol += "(" + symbol[i] + ")";
        my_string.push_back(tmp_symbol);
    }

    // deltas
    for (const delta_functions & delta : deltas) {
        std::string delta_string;

        if (is_spin_blocked)
            delta_string = delta.to_string_with_spin();
        else if (is_range_blocked)
            delta_string = delta.to_string_with_label_ranges();
        else
            delta_string = delta.to_string();

        my_string.push_back( delta_string );
    }

    // integrals
    for (auto &ints_pair : ints) {
        std::string type = ints_pair.first;
        std::vector<integrals> &ints_vec = ints_pair.second;
        for (integrals & integral : ints_vec) {
            std::string int_string;
            if (is_spin_blocked)
                int_string = integral.to_string_with_spin(type);
            else if (is_range_blocked)
                int_string = integral.to_string_with_label_ranges(type);
            else
                int_string = integral.to_string(type);
            my_string.push_back( int_string );
        }
    }

    // amplitudes
    for (auto &amps_pair : amps) {
        char type = amps_pair.first;
        std::vector<amplitudes> &amps_vec = amps_pair.second;
        for (amplitudes & amp : amps_vec) {
            std::string amp_string;
            if (is_spin_blocked)
                amp_string = amp.to_string_with_spin(type);
            else if (is_range_blocked)
                amp_string = amp.to_string_with_label_ranges(type);
            else
                amp_string = amp.to_string(type);

            my_string.push_back(amp_string );
        }
    }

    // bosons:
    for (auto && is_bdag : is_boson_dagger) {
        if ( is_bdag ) {
            my_string.emplace_back("B*");
        }else {
            my_string.emplace_back("B");
        }
    }
    if ( has_w0 ) {
        my_string.emplace_back("w0");
    }

    return my_string;
}

// copy string data, possibly excluding symbols and daggers
void pq_string::copy(void * copy_me, bool copy_daggers_and_symbols) {

    auto * in = reinterpret_cast<pq_string * >(copy_me);

    // set vacuum type
    vacuum = in->vacuum;

    // skip string?
    skip   = in->skip;

    // sign
    sign   = in->sign;

    // factor
    factor = in->factor;

    // deltas
    deltas = in->deltas;

    // integrals
    ints = in->ints;

    // amplitudes
    amps = in->amps;

    // w0
    has_w0 = in->has_w0;

    // non-summed spin labels
    non_summed_spin_labels = in->non_summed_spin_labels;

    // permutations
    permutations = in->permutations;

    // paired permutations (2)
    paired_permutations_2 = in->paired_permutations_2;

    // paired permutations (3)
    paired_permutations_3 = in->paired_permutations_3;

    // paired permutations (6)
    paired_permutations_6 = in->paired_permutations_6;

    if ( copy_daggers_and_symbols ) {

        // fermion operator symbols
        symbol = in->symbol;

        // fermion daggers
        is_dagger = in->is_dagger;

        // fermion daggers with respect to fermi vacuum
        if ( vacuum == "FERMI" ) {
            is_dagger_fermi = in->is_dagger_fermi;
        }

        // boson daggers
        is_boson_dagger = in->is_boson_dagger;
    }
}

void pq_string::set_spin_everywhere(const std::string &target, const std::string &spin) {

    // integrals
    for (auto &ints_pair : ints) {
        std::string type = ints_pair.first;
        std::vector<integrals> &ints_vec = ints_pair.second;
        for (integrals & integral : ints_vec) {
            for (size_t k = 0; k < integral.labels.size(); k++) {
                if ( integral.labels[k] == target ) {
                    integral.spin_labels[k] = spin;
                }
            }
        }
    }
    // amplitudes
    for (auto &amps_pair : amps) {
        char type = amps_pair.first;
        std::vector<amplitudes> &amps_vec = amps_pair.second;
        for (amplitudes & amp : amps_vec) {
            for (size_t k = 0; k < amp.labels.size(); k++) {
                if ( amp.labels[k] == target ) {
                     amp.spin_labels[k] = spin;
                }
            }
        }
    }
    // deltas
    for (delta_functions & delta : deltas) {
        for (size_t j = 0; j < delta.labels.size(); j++) {
            if ( delta.labels[j] == target ) {
                delta.spin_labels[j] = spin;
            }
        }
    }
}

// reset spin labels
void pq_string::reset_spin_labels() {

    // amplitudes
    for (auto &amps_pair : amps) {
        char type = amps_pair.first;
        std::vector<amplitudes> &amps_vec = amps_pair.second;
        for (amplitudes & amp : amps_vec) {
            amp.spin_labels.clear();
            for (size_t k = 0; k < amp.labels.size(); k++) {
                amp.spin_labels.emplace_back("");
            }
        }
    }
    // integrals
    for (auto &ints_pair : ints) {
        std::string type = ints_pair.first;
        std::vector<integrals> &ints_vec = ints_pair.second;
        for (integrals & integral : ints_vec) {
            integral.spin_labels.clear();
            for (size_t k = 0; k < integral.labels.size(); k++) {
                integral.spin_labels.emplace_back("");
            }
        }
    }
    // deltas
    for (delta_functions & delta : deltas) {
        delta.spin_labels.clear();
        for (size_t j = 0; j < delta.labels.size(); j++) {
            delta.spin_labels.emplace_back("");
        }
    }

    // set spins for labels in the spin map
    for (auto item : non_summed_spin_labels ) {
        std::string label = item.first;
        std::string spin = item.second;
        if ( spin == "a" || spin == "b" ) {
            // amplitudes
            for (auto &amps_pair : amps) {
                char type = amps_pair.first;
                std::vector<amplitudes> &amps_vec = amps_pair.second;
                for (amplitudes & amp : amps_vec) {
                    for (size_t k = 0; k < amp.labels.size(); k++) {
                        if ( amp.labels[k] == label ) {
                            amp.spin_labels[k] = spin;
                        }
                    }
                }
            }
            // integrals
            for (auto &ints_pair : ints) {
                std::string type = ints_pair.first;
                std::vector<integrals> &ints_vec = ints_pair.second;
                for (integrals & integral : ints_vec) {
                    for (size_t k = 0; k < integral.labels.size(); k++) {
                        if ( integral.labels[k] == label ) {
                            integral.spin_labels[k] = spin;
                        }
                    }
                }
            }
            // deltas
            for (delta_functions & delta : deltas) {
                for (size_t j = 0; j < delta.labels.size(); j++) {
                    if ( delta.labels[j] == label ) {
                        delta.spin_labels[j] = spin;
                    }
                }
            }
        }
    }
}

// set range for target label in string
void pq_string::set_range_everywhere(const std::string& target, const std::string& range) {

    // integrals
    for (auto &int_pair : ints) {
        std::string type = int_pair.first;
        for (integrals & integral : int_pair.second) {
            for (size_t k = 0; k < integral.labels.size(); k++) {
                if ( integral.labels[k] == target ) {
                    integral.label_ranges[k] = range;
                }
            }
        }
    }
    // amplitudes
    for (auto &amp_pair : amps) {
        char type = amp_pair.first;
        for (amplitudes & amp : amp_pair.second) {
            for (size_t k = 0; k < amp.labels.size(); k++) {
                if ( amp.labels[k] == target ) {
                    amp.label_ranges[k] = range;
                }
            }
        }
    }
    // deltas
    for (delta_functions & delta : deltas) {
        for (size_t j = 0; j < delta.labels.size(); j++) {
            if ( delta.labels[j] == target ) {
                delta.label_ranges[j] = range;
            }
        }
    }
}

// reset label ranges
void pq_string::reset_label_ranges(const std::unordered_map<std::string, std::vector<std::string>> &label_ranges) {

    // amplitudes
    for (auto &amp_pair : amps) {
        char type = amp_pair.first;
        for (amplitudes & amp : amp_pair.second) {
            amp.label_ranges.clear();
            for (size_t k = 0; k < amp.labels.size(); k++) {
                amp.label_ranges.emplace_back("");
            }
        }
    }
    // integrals
    for (auto &int_pair : ints) {
        std::string type = int_pair.first;
        for (integrals & integral : int_pair.second) {
            integral.label_ranges.clear();
            for (size_t k = 0; k < integral.labels.size(); k++) {
                integral.label_ranges.emplace_back("");
            }
        }
    }
    // deltas
    for (delta_functions & delta : deltas) {
        delta.label_ranges.clear();
        for (size_t j = 0; j < delta.labels.size(); j++) {
            delta.label_ranges.emplace_back("");
        }
    }

    // set ranges for non-summed labels
    for (auto item : label_ranges) {
        
        for (auto range : item.second) {

            std::string label = item.first;

            // non-summed index? not perfect logic ...
            int found = index_in_anywhere(label);
            if ( found == 1 ) {
                std::string range = item.second[0];
                if ( range == "act" || range == "ext" ) {
                    // amplitudes
                    for (auto &amp_pair : amps) {
                        char type = amp_pair.first;
                        for (amplitudes & amp : amp_pair.second) {
                            for (size_t k = 0; k < amp.labels.size(); k++) {
                                if ( amp.labels[k] == label ) {
                                    amp.label_ranges[k] = range;
                                }
                            }
                        }
                    }
                    // integrals
                    for (auto &int_pair : ints) {
                        std::string type = int_pair.first;
                        for (integrals & integral : int_pair.second) {
                            for (size_t k = 0; k < integral.labels.size(); k++) {
                                if ( integral.labels[k] == label ) {
                                    integral.label_ranges[k] = range;
                                }
                            }
                        }
                    }
                    // deltas
                    for (delta_functions & delta : deltas) {
                        for (size_t j = 0; j < delta.labels.size(); j++) {
                            if ( delta.labels[j] == label ) {
                                delta.label_ranges[j] = range;
                            }
                        }
                    }
                }else {
                    printf("\n");
                    printf("    error: invalid range %s\n", range.c_str());
                    printf("\n");
                    exit(1);
                }
            }
        }
    }
}


// set labels for integrals
void pq_string::set_integrals(const std::string &type, const std::vector<std::string> &in, std::vector<std::string> op_portions) {
    integrals new_ints;
    new_ints.labels = in;
    new_ints.op_portions = op_portions;
    new_ints.sort();
    ints[type].push_back(new_ints);
}

// set labels for amplitudes
void pq_string::set_amplitudes(char type, int n_create, int n_annihilate, int n_ph, const std::vector<std::string> &in, std::vector<std::string> op_portions) {
    amplitudes new_amps;
    new_amps.labels = in;
    new_amps.n_create = n_create;
    new_amps.n_annihilate = n_annihilate;
    new_amps.n_ph = n_ph;
    new_amps.op_portions = op_portions;
    new_amps.sort();
    amps[type].push_back(new_amps);
}

// how many times does an index appear amplitudes, deltas, integrals, and operators?
int pq_string::index_in_anywhere(const std::string &idx) {

    // find index in deltas
    int n = index_in_deltas(idx, deltas);

    // find index in integrals
    for (const auto & int_pair : ints) {
        const std::string &type = int_pair.first;
        const std::vector<integrals> &ints = int_pair.second;
        n += index_in_integrals(idx, ints);
    }

    // find index in amplitudes
    for (const auto & amp_pair : amps) {
        const char &type = amp_pair.first;
        const std::vector<amplitudes> &amps = amp_pair.second;
        n += index_in_amplitudes(idx, amps);
    }

    // find index in operators
    n += index_in_operators(idx, symbol);

    return n;
}

}
