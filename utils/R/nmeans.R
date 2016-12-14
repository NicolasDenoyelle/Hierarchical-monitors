logLik <- function(x, fit){
  result = 0
  
  #for each center compute within points probability
  for(i in 1:nrow(fit$centers)){
    points = scale(x[fit$cluster==i,], center=TRUE, scale=TRUE)
    sigma  = attr(points, "scaled:scale")
    C = nrow(points)/(nrow(x)*sigma*sqrt(2*pi))
    P = sum(exp(points*points*replicate(nrow(points), 0.5))*replicate(nrow(points),C))
    result = result + P
  }
  return(log(result))
}

kmeansBIC = function(x, fit){
   return(-logLik(x,fit) + log(nrow(x)) * ncol(fit$centers)*nrow(fit$centers) * 0.5)
}

nmeans <- function(x, max_centers=20, iter.max=10, algorithm = c("Hartigan-Wong", "Lloyd", "Forgy", "MacQueen"), trace=FALSE){
  dim    = ncol(x)
  NMEANS = NULL
  BIC    = Inf
  center = kmeans(x, 1, iter.max=iter.max, nstart = 1, algorithm=algorithm, trace=trace)$center

  #increase the number of centers until max center
  for(k in 1:max_centers){
    centers = k
    nstart = k
    if(k>2){
      center = colSums(fit$centers)/replicate(dim, k-1)
      centers = matrix(data = fit$centers, nrow = k, ncol = dim)
      centers[k,] = center
    }

    fit = kmeans(x, centers = centers, iter.max=iter.max, nstart = nstart, algorithm=algorithm, trace=trace)
    new_BIC = kmeansBIC(x, fit)
    if(new_BIC<BIC){
      BIC = new_BIC
      NMEANS = fit
    }
  }
  NMEANS
}

