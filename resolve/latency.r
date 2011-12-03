library(gplots)
library(RSvgDevice)  

matrix.axes <- function(data) {
    a <- ((1:48)-1) / 48
    x <- (1:48) - 1
    axis(side=1, at=a, labels=x, las=1)
    axis(side=2, at=a, labels=x, las=3);
}

plot_grid <- function (arch, test) {
  inp <- as.matrix(read.table(sprintf("%s.%s.lat.csv", arch, test)))
  ncols <- 11
  col <- colorpanel(ncols, "white", "grey10")
  r <- range(inp) # autoscale
  t <- sprintf("%s %s (range %f)", arch, test, max(inp) - min(inp))
  filled.contour(inp, zlim=r, plot.axes=matrix.axes(inp), nlevels=ncols, title=title(t))
}

for (arch in c("48native", "48xenpin", "48xennopin")) {
  for (test in c("mempipe_lat", "pipe_lat", "unix_lat", "tcp_lat")) {
    f <- sprintf("latency-%s-%s.pdf", arch, test)
    pdf(file=f)
    plot_grid(arch, test)
  }
}
