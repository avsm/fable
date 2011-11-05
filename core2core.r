plot_fields <- function (subdir,test) {
  g <- read.table(sprintf("archive/%s/%s.csv", subdir, test))
  m <- as.matrix(g)
  a <- mean(m)
  image(m)
  title(subdir, sub=sprintf("%s raw", test), cex.main=0.9)
  for (i in 1:dim(m)[1])
    m[i,i] <- a
  image(m)
  title(subdir, sub=sprintf("%s norm", test), cex.main=0.9)
}

subdirs=c("native-48core-linux", "xen-48core-linux-dom0")

pdf(file="core2core.pdf", paper="a4")
tests=c("pipe_thr","pipe_lat","tcp_thr","tcp_lat","unix_thr","unix_lat","mempipe_thr")
for (s in subdirs)  {
  par(mfrow=c(4,4))
  for (t in tests) plot_fields(s,t)
}
