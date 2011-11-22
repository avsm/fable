(*
 * Copyright (c) 2011 Anil Madhavapeddy <anil@recoil.org>
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
open Printf
open Scanf
open Results

(* test parameters *)
let os = "native48" 
let basedir = "../results.sizes.native48"
let sizes = [ 128; 4096; 65536 ]
let modes = [ "thr"; "lat" ]
let tests = [ "pipe"; "unix"; "tcp"; "tcp_nodelay"; "mempipe"; "vmsplice_pipe" ]
let ncores = 48
(* results array is a list of data points *)
let res : results = Array.make_matrix ncores ncores []

(* progress bar *)
let pctr = ref 0
let ping () =
  incr pctr;
  if (!pctr mod 100) = 0 then fprintf stderr "%d\n%!" !pctr

let parse_tsc transformfn fname =
  let fin = open_in fname in
  let points = ref [] in
  (try
     while true do
       points := (transformfn (float_of_string (input_line fin))) :: !points;
    done with End_of_file -> close_in fin);
  Stats.std_dev !points

(* Parse a test directory and add it to the results matrix *)
let parse ~c1 ~c2 ~mode ~size ~test =
  let file = sprintf "%s/%d/%d-%d-%s_%s/01-%s_%s-headline.log"
    basedir size c1 c2 test mode test mode in
  let rawtsc = sprintf "%s/%d/%d-%d-%s_%s/01-%s_%s-raw_tsc.log"
    basedir size c1 c2 test mode test mode in
  fprintf stderr "%s\n%!" file;
  try
    let fin = open_in file in
    let r = match mode with
    |"thr" ->
      fscanf fin "%s %d %d %f Mbps"
       (fun name size count result ->
         let stddev = parse_tsc
           (fun r -> (float (size) *. 8.) /. (r *. 1e6)) rawtsc in
         { test; size; mode; count; result; stddev }
       )
    |"lat" ->
      let stddev = parse_tsc (fun r -> r) rawtsc in
      fscanf fin "%s %d %d %fs"
       (fun name size count result ->
         { test; size; mode; count; result; stddev }
       )
    |_ -> assert false
    in close_in fin;
    ping ();
    res.(c1).(c2) <- r :: res.(c1).(c2)
  with Sys_error e -> fprintf stderr "err %s!\n%!" e

let _ =
  List.iter (fun test ->
    List.iter (fun size ->
      List.iter (fun mode ->
        for c1 = 0 to ncores - 1 do
          for c2 = 0 to ncores - 1 do
            parse ~c1 ~c2 ~mode ~size ~test
          done
        done
      ) modes
    ) sizes
  ) tests;
  let fname = sprintf "results.%s.marshal" os in
  fprintf stderr "output file: %s\n%!" fname;
  let fout = open_out fname in
  Marshal.to_channel fout res [];
  close_out fout
