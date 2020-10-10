//
// pdaggerq - A code for bringing strings of creation / annihilation operators to normal order.
// Filename: data.h
// Copyright (C) 2020 A. Eugene DePrince III
//
// Author: A. Eugene DePrince III <adeprince@fsu.edu>
// Maintainer: DePrince group
//
// This file is part of the pdaggerq package.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef DATA_H
#define DATA_H

namespace pdaggerq {

class StringData {

  private:


  public:

    StringData(){};
    ~StringData(){};

    /// factor
    double factor = 1.0;

    /// list: labels for fermionic creation / annihilation operators 
    std::vector<std::string> string;

    /// list: labels for 1- or 2-index tensor
    std::vector<std::string> tensor;

    /// list: labels t amplitudes
    std::vector<std::vector<std::string> > t_amplitudes;

    /// list: labels left-hand eom-cc amplitudes
    std::vector<std::vector<std::string> > left_amplitudes;

    /// list: labels right-hand amplitudes
    std::vector<std::vector<std::string> > right_amplitudes;

    /// should we account for l0 in EOM-CC?
    bool has_l0;

    /// should we account for r0 in EOM-CC?
    bool has_r0;

};

}

#endif
