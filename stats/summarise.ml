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
open Results

(* Output an HTML grid applied against the matrix *)
let grid oc res fn =
  fprintf oc "<table>\n";
  Array.iteri (fun c1 col ->
    fprintf oc " <tr>\n";
    Array.iteri (fun c2 allres ->
      fprintf oc "  <td>";
      fprintf oc (fn allres);
      fprintf oc "  </td>\n"
    ) col;
    fprintf oc " </tr>\n";  
  ) res;
  fprintf oc "</table>"

(* Obtain a single results grid for a given test, size, mode *)
let get_grid ~test ~size ~mode res =
  let len = Array.length res in
  let g = Array.make_matrix len len (0.,0.) in
  Array.iteri (fun c1 col ->
    Array.iteri (fun c2 allres ->
      match List.filter (fun r ->
        r.test = test && r.size = size && r.mode=mode) allres with
      |[{result;stddev}] -> g.(c1).(c2) <- (result, stddev)
      |x -> failwith (sprintf "%d results %s %d %s" (List.length x) test size mode)
    ) col
  ) res;
  g 

(* Get list of test names for a given mode. We assume all tests are run on 
   all cores *)
let get_test_names ~mode res =
  List.fold_left (fun a b ->
    if b.mode = mode then
     (if List.mem b.test a then a else b.test :: a)
    else
     a
  ) [] res.(0).(0)

let show_test ~sizes ~tests ~mode ~use oc res =
  fprintf oc "TEST: %s (%s)\n" mode
    (match use with
     |`all -> "all results"
     |`no_diag -> "discarding same core"
     |`diag -> "only same core"
    );
  fprintf oc "%-20s %!" "Test/Req size";
  List.iter (fun sz -> fprintf oc "%-20s" (string_of_int sz)) sizes;
  fprintf oc "\n%!";
  List.iter (fun test ->
    fprintf oc "%-20s %!" (test ^ "_thr");
    List.iter (fun size ->
      let grid = get_grid ~test ~size ~mode res in
      match mode with
      |"lat" ->
        let mean = Stats.mean_grid ~use grid (fun (a,b) -> a *. 1e6) in
        let stddev = Stats.mean_grid ~use grid (fun (a,b) -> b *. 1e6) in
        fprintf oc "%-20s" (sprintf "%.2f (%.2f)" mean stddev)
      |"thr" ->
        let mean = Stats.mean_grid ~use grid fst in
        let stddev = Stats.mean_grid ~use grid snd in
        fprintf oc "%-20s" (sprintf "%.2f (%.2f)" mean stddev)
      |_ -> assert false
    ) sizes;
    fprintf stderr "\n%!"
  ) tests;
  fprintf oc "-----\n%!"
 
let _ =
  let fname = "results.native48.marshal" in
  let fin = open_in fname in
  let res : results = Marshal.from_channel fin in
  close_in fin;

  let ncores = Array.length res in
  fprintf stderr "number of cores: %d\n\n%!" ncores;

  (* First, summarise throughput across all cores *)
  let oc = stderr in
  let sizes = [ 128; 4096; 65536 ] in
  List.iter (fun use ->
    let mode = "thr" in
    let tests = get_test_names mode res in
    show_test ~sizes ~tests ~mode ~use oc res;

    let mode = "lat" in
    let tests = get_test_names mode res in
    show_test ~sizes ~tests ~mode ~use oc res;
  ) [`all; `no_diag; `diag];

  ()
