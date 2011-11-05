plot_fields <- function (test) {
  g <- read.table(sprintf("collected/%s.csv", test))
  m <- as.matrix(g)
  a <- mean(m)
  for (i in 1:48) m[i,i] <- a
  image(m)
}

par(mfrow=c(3,4))
tests=c("pipe_thr","pipe_lat","tcp_thr","tcp_lat","unix_thr","unix_lat","mempipe_thr")
for (t in tests) plot_fields(t)
