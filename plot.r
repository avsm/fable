plot_fields <- function (inp, lats, xlab, ylab, t) {
  nlats <- length(lats)
  all_lats <- subset(inp, name %in% lats)
  xrange <- range(all_lats$size) 
  yrange <- range(all_lats$result)
  plot(xrange, yrange, type="n", xlab=xlab, ylab=ylab)
  title(t)
  colors <- rainbow(nlats) 
  linetype <- c(1:nlats) 
  plotchar <- seq(18,18+nlats,1)
  for (i in 1:nlats) { 
    tree <- subset(inp, name==lats[i]) 
    lines(tree$size, tree$result, type="b", lwd=1.5,
      lty=linetype[i], col=colors[i], pch=plotchar[i]) 
  } 
  legend(xrange[1], yrange[2], lats, cex=0.8, col=colors,
  	 pch=plotchar, lty=linetype)
}

inp_same <- read.table("results-same.csv", header=T)
inp_diff <- read.table("results-diff.csv", header=T)

par(mfrow=c(2,2))
plot_fields(inp_same, c("unix_lat", "pipe_lat", "tcp_lat"), "request size (bytes)", "avg latency (us)", "Latency (same CPU)");
plot_fields(inp_same, c("unix_thr", "pipe_thr", "tcp_thr"), "request size (bytes)", "avg throughput (Mbs)", "Throughput (same CPU)")
plot_fields(inp_diff, c("unix_lat", "pipe_lat", "tcp_lat"), "request size (bytes)", "avg latency (us)", "Latency (different CPU)");
plot_fields(inp_diff, c("unix_thr", "pipe_thr", "tcp_thr"), "request size (bytes)", "avg throughput (Mbs)", "Throughput (different CPU)")
