(*
 * Copyright (c) 2004 Anil Madhavapeddy <anil@recoil.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *)

let mean l =
  let len = float_of_int (List.length l) in
  let sum = List.fold_left (+.) 0. l in
  sum /. len

(* Given a results array and a function to convert a node to a float,
   calculate the mean *)
let mean_grid g fn =
  let sum = Array.fold_left (fun a rows ->
    a +. (Array.fold_left (fun a r ->
      a +. (fn r)) 0. rows)) 0. g in
  let len = (float) ((Array.length g) * (Array.length g)) in
  sum /. len

(* Given a results array, calculate the mean but ignore the centre diagonal
   when x=y *)
let mean_grid ?(use=`all) g fn =
  let sum = ref 0. in
  Array.iteri (fun c1 col ->
    Array.iteri (fun c2 allres ->
      match use with
      |`all -> sum := !sum +. (fn allres)
      |`no_diag -> if c1 != c2 then sum := !sum +. (fn allres)
      |`diag -> if c1 = c2 then sum := !sum +. (fn allres)
    ) col
  ) g;
  let len =
    match use with
    |`all -> (Array.length g) * (Array.length g)
    |`no_diag -> (Array.length g) * (Array.length g - 1)
    |`diag -> Array.length g
  in
  !sum /. (float len)
 
let std_dev l =
  let fl = float_of_int (List.length l) in
  let sqdev s = ((mean l) -. s) ** 2. in
  let sumsqdev = List.fold_left (fun a b -> a +. (sqdev b)) 0. l in
  sqrt (sumsqdev /. (fl -. 1.))

let std_moment e l =
  let fl = float_of_int (List.length l - 1) in
  let raisedev s = ((mean l) -. s) ** e in
  let sumdev = List.fold_left (fun a b -> a +. (raisedev b)) 0. l in
  let stddevraised = (std_dev l) ** e in
  sumdev /. (fl *. stddevraised)

(* skewness measures the left/right of normal, kurtosis is peak of normal,
    see http://www.itl.nist.gov/div898/handbook/eda/section3/eda35b.htm *)
let skewness = std_moment 3.
let kurtosis l = (std_moment 4. l) -. 3.

(* variance of a data set is std_dev squared *)
let variance l =
  let sd = std_dev l in
  sd *. sd

(* standard deviation / root n *)
let avg_error l =
  let fl = float_of_int (List.length l) in
  (std_dev l) /. (sqrt fl)

(* see http://www.inf.ethz.ch/personal/gutc/lognormal/maths/node10.html *)
let log_mean l =
  let mu = mean l in
  let sigmasq = (std_dev l) ** 2. in
  exp (mu +. (sigmasq /. 2.))

(* see http://www.inf.ethz.ch/personal/gutc/lognormal/maths/node10.html *)
let log_sigma l =
  let mu = mean l in
  let sigmasq = (std_dev l) ** 2. in
  let a = (exp sigmasq) -. 1. in
  (exp ((2. *. mu) +. sigmasq)) *. a

(* calculate z-score for an observation given stddev and mean *)
let zscore stddev mean x =
  (x -. mean) /. stddev
