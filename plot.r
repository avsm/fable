inp <- read.table("results.csv", header=T)

plot_fields <- function (lats, xlab, ylab, t) {
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

par(mfrow=c(1,2))
plot_fields(c("unix_lat", "pipe_lat", "tcp_lat"), "request size (bytes)", "avg latency (us)", "Latency");
plot_fields(c("unix_thr", "pipe_thr", "tcp_thr"), "request size (bytes)", "avg throughput (Mbs)", "Throughput")
