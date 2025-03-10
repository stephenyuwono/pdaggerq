//
// pdaggerq - A code for bringing strings of creation / annihilation operators to normal order.
// Filename: linkage.cc
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

#include <memory>
#include <utility>
#include <cstring>
#include <stack>
#include <numeric>
#include <cmath>
#include "../include/linkage.h"
#include "../include/linkage_set.hpp"

namespace pdaggerq {

    LinkagePtr Linkage::link(const vertex_vector &op_vec) {
        if (op_vec.empty()) return make_shared<Linkage>(); // return an empty linkage if the vector is empty

        VertexPtr linkage = make_shared<Vertex>(); // initialize the linkage as an empty vertex
        for (const auto & op : op_vec) {

            // skip empty vertices
            if (op->empty()) continue;

            // skip constants with a value of 1
            if (op->is_constant()) {
                if (fabs(op->value() - 1.0) < 1e-8) continue;
            }

            // set the first vertex as the linkage
            if (linkage->empty()) {
                linkage = op;
                continue;
            }

            //link the rest of the vertices
            linkage = linkage * op;
        }

        // if no linkage was made, return self
        if (linkage->empty()) return as_link(linkage);

        // vertex found, but not linked, so return a linkage with the vertex and one
        if (!linkage->is_linked()) return as_link(1.0 * linkage);

        // return the linkage
        return as_link(linkage);
    }

    pair<vector<shape>, vector<shape>> Linkage::scales(bool fully_expand) const {
        // initialize scaling maps
        vector<shape> flops, mems;

        // return empty scaling if the root is a temp and we are not fully expanding
        if (empty() || (is_temp() && !fully_expand))
            return {flops, mems};

        // use depth to reserve space for scaling
        flops.reserve(depth_+1);
        mems.reserve(depth_+1);

        // add the scaling of the left vertex
        if (left_->is_linked()) {
            auto [left_flops, left_mems] = as_link(left_)->scales(fully_expand);
            flops.insert(flops.end(), left_flops.begin(), left_flops.end());
            mems.insert(mems.end(), left_mems.begin(), left_mems.end());
        }

        // add the scaling of the right vertex
        if (right_->is_linked()) {
            auto [right_flops, right_mems] = as_link(right_)->scales(fully_expand);
            flops.insert(flops.end(), right_flops.begin(), right_flops.end());
            mems.insert(mems.end(), right_mems.begin(), right_mems.end());
        }

        // add the scaling of the root vertex if neither left nor right are empty, constant, or temps
        if (!left_->empty() && !right_->empty() && !left_->is_constant() && !right_->is_constant()) {
            flops.push_back(flop_scale_);
            mems.push_back(mem_scale_);
        }

        // return the total scaling
        return {flops, mems};
    }

    vertex_vector Linkage::link_vector(bool regenerate, bool fully_expand) const {

        vertex_vector result;
        {
            // Lock the mutex for this scope
            std::lock_guard<std::mutex> lock(mtx_);
            result = fully_expand ? all_vert_ : link_vector_;
        }

        // if the link vector is already generated and does not need to be regenerated, return it
        if (!regenerate && !result.empty())
            return result;

        // else regenerate the result vector
        result.clear();
        result.reserve(2*(depth_+1));

        // add operators from the left (excluding additions for now)
        if (left_->is_linked() && !left_->empty()) {
            if (!left_->is_expandable() && !fully_expand) {
                result.push_back(left_); // if not fully expanding, add if an intermediate
            } else {
                const vertex_vector &left_vec = left_->link_vector(regenerate, fully_expand);
                result.insert(result.end(), left_vec.begin(), left_vec.end());
            }
        } else if (!left_->empty() && (fabs(left_->value() - 1.0) > 1e-8)) {
            result.push_back(left_);
        }

        // add operators from the right (excluding additions for now)
        if (right_->is_linked() && !right_->empty()) {
            if (!right_->is_expandable() && !fully_expand) {
                result.push_back(right_); // if not fully expanding, add if an intermediate
            } else {
                const vertex_vector &right_vec = right_->link_vector(regenerate, fully_expand);
                result.insert(result.end(), right_vec.begin(), right_vec.end());
            }
        } else if (!right_->empty() && (fabs(right_->value() - 1.0) > 1e-8)){
            result.push_back(right_);
        }

        // copy the result vector to the link_vector if the size is less than 32
        bool store_vector = !low_memory_;
        if (store_vector) {
            // Lock the mutex for this scope
            std::lock_guard<std::mutex> lock(mtx_);
            if (fully_expand)
                 all_vert_ = result;
            else link_vector_ = result;
        } else if (fully_expand && !all_vert_.empty()) {
            // if not storing the vector, clear the all_vert_ vector
            std::lock_guard<std::mutex> lock(mtx_);
            all_vert_.clear();
        } else if (!fully_expand && !link_vector_.empty()) {
            // if not storing the vector, clear the link_vector_ vector
            std::lock_guard<std::mutex> lock(mtx_);
            link_vector_.clear();
        }

        // return the result vector
        return result;
    }

    vertex_vector Linkage::vertices(bool regenerate) const {
        return link_vector(regenerate, true);
    }

    bool Linkage::factor() {
        // TODO: implement factorization on the linkage level
        return false;
    }

    vertex_vector  Linkage::find_links(const VertexPtr &target_vertex, long search_depth) const {

        if (target_vertex == nullptr) return {}; // if the target vertex is null, return an empty vector
        if (this->depth() < target_vertex->depth()) return {}; // if the target vertex is deeper, it cannot be found

        bool found = *target_vertex == *this;
        if (found) return {shallow()}; // if this is the target vertex, return it

        vertex_vector links;

        if (search_depth > 0 || search_depth == -1) {
            search_depth = search_depth == -1 ? -1 : search_depth - 1;
            if (left_->is_linked()) {
                const auto &left_links = as_link(left_)->find_links(target_vertex, search_depth);
                links.insert(links.end(), left_links.begin(), left_links.end());
            }
            if (right_->is_linked()) {
                const auto &right_links = as_link(right_)->find_links(target_vertex, search_depth);
                links.insert(links.end(), right_links.begin(), right_links.end());
            }
        }
        return links;
    }

    vertex_vector Linkage::find_scalars() const {
        vertex_vector scalars;
        if (left_->empty() && right_->empty())
            return scalars; // do not add empty vertices

        if (is_temp())
            return scalars; // do not add temps

        // if this is a scalar and the left nor right are empty, add this to the scalars
        bool left_valid  =  !left_->empty() && (fabs( left_->value() - 1.0) > 1e-8);
        bool right_valid = !right_->empty() && (fabs(right_->value() - 1.0) > 1e-8);
        if (is_scalar() && left_valid && right_valid)
            scalars.push_back(this->shallow());

        if (left_->is_linked() && left_valid) {
            const auto &left_scalars = as_link(left_)->find_scalars();
            scalars.insert(scalars.end(), left_scalars.begin(), left_scalars.end());
        }

        if (right_->is_linked() && right_valid) {
            const auto &right_scalars = as_link(right_)->find_scalars();
            scalars.insert(scalars.end(), right_scalars.begin(), right_scalars.end());
        }

        return scalars;
    }

    pair<VertexPtr, bool> Linkage::replace(const VertexPtr &target_vertex, const VertexPtr &new_vertex) const {

        if (!target_vertex || !new_vertex) return {shallow(), false};


        bool replaced = *target_vertex == *this;
        if (replaced) return {new_vertex, true}; // this is the target vertex, so replace it

        if (depth_ < target_vertex->depth())
            return {shallow(), false}; // if the target vertex is deeper, it cannot be replaced

        VertexPtr new_left = left_->shallow(), new_right = right_->shallow();
        if (left_->is_linked()) {
            const auto &[replaced_left, left_found] = as_link(left_)->replace(target_vertex, new_vertex);
            if (left_found) {
                new_left = replaced_left;
                replaced = true;
            }
        }
        if (right_->is_linked()) {
            const auto &[replaced_right, right_found] = as_link(right_)->replace(target_vertex, new_vertex);
            if (right_found) {
                new_right = replaced_right;
                replaced = true;
            }
        }

        if (!replaced)
            return {shallow(), false}; // no replacements were made. return the original linkage

        // replacement was made, so create a new linkage with the replaced vertices
        MutableLinkagePtr new_link = as_link(is_addition() ? new_left + new_right : new_left * new_right);
        new_link->copy_misc(*this); // copy misc properties
        return {new_link, true};
    }

    pair<VertexPtr, bool> Linkage::replace_id(const VertexPtr &target_vertex, long new_id) const {
        if (!target_vertex) return {shallow(), false};

        bool replaced = same_temp(target_vertex);
        if (replaced) {
            MutableVertexPtr replacement = shallow();
            as_link(replacement)->id_ = new_id;
            return {replacement, true}; // this is the target vertex, so replace it
        }

        replaced = false;
        VertexPtr new_left = left_->shallow(), new_right = right_->shallow();
        if (left_->is_linked()) {
            const auto &[replaced_left, left_found] = as_link(left_)->replace_id(target_vertex, new_id);
            if (left_found) {
                new_left = replaced_left;
                replaced = true;
            }
        }
        if (right_->is_linked()) {
            const auto &[replaced_right, right_found] = as_link(right_)->replace_id(target_vertex, new_id);
            if (right_found) {
                new_right = replaced_right;
                replaced = true;
            }
        }

        if (!replaced)
            return {shallow(), false}; // no replacements were made. return the original linkage

        // replacement was made, so create a new linkage with the replaced vertices
        MutableLinkagePtr replacement = as_link(shallow());
        replacement->left_  = new_left;
        replacement->right_ = new_right;

        replacement->forget(); // forget the linkage memory
        replacement->set_properties();

        return {replacement, true};
    }

    void Linkage::replace_lines(const unordered_map<Line, Line, LineHash> &line_map, bool update_name) {

        // call the base class replace_lines to replace the lines of the root vertex
        Vertex::replace_lines(line_map, update_name);

        // recursively replace the lines of the left and right vertices
        MutableVertexPtr new_left = left_->shallow(), new_right = right_->shallow();
        new_left->replace_lines(line_map, update_name);
        new_right->replace_lines(line_map, update_name);

        // set the new left and right vertices
        left_ = new_left;
        right_ = new_right;

        // recursively forget the linkage memory
        forget();

        // reset the links
        build_connections();
    }

    bool Linkage::has_temp(const VertexPtr &temp, bool enter_temps, long search_depth) const {
        if (temp->is_linked() && depth_ < as_link(temp)->depth_)
            return false; // if the depth of the temp is greater than this linkage, there cannot be a match

        if (same_temp(temp)) return true;
        if (is_temp() && !enter_temps) return false;

        // recursively check if left and right vertices have the temp up to a certain search_depth
        if (search_depth > 0 || search_depth == -1) {
            search_depth = search_depth == -1 ? -1 : search_depth - 1;
            if (left_->is_linked())
                if (as_link(left_)->has_temp(temp, enter_temps, search_depth)) return true;
            if (right_->is_linked())
                if (as_link(right_)->has_temp(temp, enter_temps, search_depth)) return true;
        }
        return false;
    }

    bool Linkage::has_link(const VertexPtr &link, bool enter_temps, long search_depth) const {
        if (link->is_linked() && depth_ < as_link(link)->depth_)
            return false; // if the depth of the temp is greater than this linkage, there cannot be a match

        if (*this == *link) return true;
        if (is_temp() && !enter_temps) return false;

        // recursively check if left and right vertices have the temp up to a certain search_depth
        if (search_depth > 0 || search_depth == -1) {
            search_depth = search_depth == -1 ? -1 : search_depth - 1;
            if (left_->is_linked())
                if (as_link(left_)->has_link(link, enter_temps, search_depth)) return true;
            if (right_->is_linked())
                if (as_link(right_)->has_link(link, enter_temps, search_depth)) return true;
        }
        return false;
    }

    bool Linkage::has_any_temp() const {
        if (is_temp()) return true;
        if (left_->is_linked() && as_link(left_)->has_any_temp()) return true;
        if (right_->is_linked() && as_link(right_)->has_any_temp()) return true;
        return false;
    }

    vertex_vector Linkage::get_temps(bool enter_temps, bool enter_additions) const {
        vertex_vector temps;

        if (is_temp()) {
            temps.push_back(shallow());
            if (!enter_temps) return temps;
        }
        if (is_addition() && !enter_additions) {
            return temps;
        }

        if (left_->is_linked()) {
            const auto &left_temps = as_link(left_)->get_temps(enter_temps, enter_additions);
            temps.insert(temps.end(), left_temps.begin(), left_temps.end());
        }

        if (right_->is_linked()) {
            const auto &right_temps = as_link(right_)->get_temps(enter_temps, enter_additions);
            temps.insert(temps.end(), right_temps.begin(), right_temps.end());
        }

        // sort temps by id
        std::sort(temps.begin(), temps.end(), [](const VertexPtr &a, const VertexPtr &b) {
            return a->id() < b->id();
        });

        return temps;
    }

    idset Linkage::get_ids(const string &type) const {
        idset ids;
        if (is_temp()) {
            if (this->type() == type || type == "any")
                ids.insert(id_);
            return ids;
        }

        if (left_->is_linked()) {
            const auto &left_ids = as_link(left_)->get_ids(type);
            ids.insert(left_ids.begin(), left_ids.end());
        }
        if (right_->is_linked()) {
            const auto &right_ids = as_link(right_)->get_ids(type);
            ids.insert(right_ids.begin(), right_ids.end());
        }

        return ids;
    }

    linkage_vector Linkage::permutations(bool regenerate) const {

        // initialize the result vector

        linkage_vector result;

        {
            // Lock the mutex for this scope
            std::lock_guard<std::mutex> lock(mtx_);
            result = permutations_;
        }

        // if the result vector is already generated and does not need to be regenerated, return it
        if (!regenerate && !result.empty())
            return result;

        if (empty()) {
            result.clear();
            return result;
        }


        // initialize the result vector with the identity permutation
        result = {as_link(shallow())};
        result.reserve(2*(depth_+1)); // reserve space for the result vector

        // do not generate permutations for temps (their structure is fixed)
        if (is_temp()) return result;

        // do not store permutations if the depth is too large
        bool store_permutations = !low_memory_;

        if (left_->empty() || right_->empty()) {
            if (left_->empty() && right_->is_linked()) {
                // return permutations of the right vertex if the left vertex is empty
                result = as_link(right_->shallow())->permutations();
            } else if (right_->empty() && left_->is_linked()) {
                // return permutations of the left vertex if the right vertex is empty
                result = as_link(left_->shallow())->permutations();
            }

            // return the identity permutation if one of the vertices is empty
            return result;
        }

        // additions are special cases, so we need to handle them separately
        if (is_addition()) {
            // only consider the best permutation of the left and right vertices
            const LinkagePtr &left_perm = as_link(left_)->best_permutation();
            const LinkagePtr &right_perm = as_link(right_)->best_permutation();

            // check if the left and right vertices are the same
            bool same_left_right = *left_perm == *right_perm;

            // if the left and right vertices are not the same, add the left and right permutations
            if (!same_left_right) {
                result.push_back(as_link(left_perm + right_perm));
                result.push_back(as_link(right_perm + left_perm));
            }

            // copy the result vector to the permutations_ vector only if low memory mode is off
            if (store_permutations) {
                // Lock the mutex for the scope
                std::lock_guard<std::mutex> lock(mtx_);
                permutations_ = result;
            } else if (!permutations_.empty()) {
                // if not storing permutations, clear the permutations_ vector
                std::lock_guard<std::mutex> lock(mtx_);
                permutations_.clear();
            }

            return result;
        }

        // generate all permutations of the link vector
        const vertex_vector &link_vec = link_vector(regenerate);
        vector<size_t> idxs(link_vec.size());
        std::iota(idxs.begin(), idxs.end(), 0);

        // generate all permutations of the link vector
        while (std::next_permutation(idxs.begin(), idxs.end())) {
            // generate this permutation
            vertex_vector link_perm(link_vec.size());
            std::transform(idxs.begin(), idxs.end(), link_perm.begin(), [&link_vec](size_t i) {
                return link_vec[i];
            });

            result.push_back(link(link_perm));
        }

        // copy the result vector to the permutations_ vector only if low memory mode is off
        if (store_permutations) {
            // Lock the mutex for the scope
            std::lock_guard<std::mutex> lock(mtx_);
            permutations_ = result;
        } else if (!permutations_.empty()) {
            // if not storing permutations, clear the permutations_ vector
            std::lock_guard<std::mutex> lock(mtx_);
            permutations_.clear();
        }
        return result;

    }

    LinkagePtr Linkage::best_permutation() const {

        // initialize the best permutation as the current linkage
        LinkagePtr best_perm = as_link(shallow());

        // generate every permutation
        const linkage_vector &all_perms = permutations();
        if (all_perms.size() <= 1) {
            // if no permutations, return this as the best permutation
            return best_perm;
        }

        // test scaling of each permutation
        auto [best_flops, best_mems] = best_perm->netscales();
        for (const auto &perm : all_perms) {
            auto [flops, mems] = perm->netscales();

            // check if flops current permutation is better than best permutation
            int scaling_check = flops.compare(best_flops);

            bool make_best = scaling_check == scaling_map::this_better;
            if (!make_best && scaling_check == scaling_map::this_same) {
                // if flops are the same, check mems
                scaling_check = mems.compare(best_mems);
                make_best = scaling_check == scaling_map::this_better;

                // if the mems are the same, check the lines
                if (!make_best && scaling_check == scaling_map::this_same) {
                    make_best = perm->lines() < best_perm->lines();
                }

                // if lines are the same, use string representation of names
                if (!make_best && scaling_check == scaling_map::this_same) {
                    make_best = perm->name() < best_perm->name();
                }
            }

            if (make_best) {
                best_flops = flops;
                best_mems = mems;
                best_perm = perm;
            }
        }

        // return the best permutation
        return best_perm;
    }

    linkage_vector Linkage::subgraphs(size_t max_depth) const {

        if (is_temp()) { // do not generate subgraphs for temps
            return {as_link(shallow())};
        }
        if (empty()) { // return an empty vector if the linkage is empty
            return {};
        }

        // build permutations of root vertex
        linkage_vector top_perms = {as_link(shallow())};
        linkage_set unique_subgraphs; unique_subgraphs.reserve(4 * (depth_+1));

        // now add the subgraphs of the left and right vertices
        for (const auto &perm : top_perms) {
            if (perm->empty()) continue; // skip empty vertices
            if (perm->depth() <= max_depth)
                unique_subgraphs.insert(perm);

            if (perm->left_->is_linked() && !perm->left_->empty()) {
                auto left_perms = as_link(perm->left_)->subgraphs(max_depth);
                unique_subgraphs.insert(left_perms.begin(), left_perms.end());
            }
            if (perm->right_->is_linked() && !perm->right_->empty()) {
                auto right_perms = as_link(perm->right_)->subgraphs(max_depth);
                unique_subgraphs.insert(right_perms.begin(), right_perms.end());
            }
        }

        // copy the unique subgraphs as a vector and return it
        return {unique_subgraphs.begin(), unique_subgraphs.end()};
    }
}