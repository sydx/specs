#include <iostream>
#include <RcppArmadillo.h>

using namespace Rcpp;
using namespace arma;

// [[Rcpp::depends(RcppArmadillo)]]

double st(double x, double y) {
    return arma::sign(x)*std::max(0.0,std::abs(x) - y);
} //Soft-thresholding

// [[Rcpp::export]]
arma::mat diff_mat (arma::mat x) {
    return x.rows(arma::span(1,x.n_rows-1)) - x.rows(arma::span(0,x.n_rows-2));
} //matrix diff

// [[Rcpp::export]]
arma::mat embed_mat (arma::mat x,const int p=1) {
    //std::cout << "START embed_mat" << std::endl;
    //std::cout << "x: " << x << std::endl;
    //std::cout << "p: " << p << std::endl;
    int n = x.n_rows;
    int m = x.n_cols;
    //std::cout << "n: " << n << std::endl;
    //std::cout << "m: " << m << std::endl;
    arma::mat y(n-p+1,p*m,arma::fill::zeros);
    //std::cout << "y: " << y << std::endl;
    for(int j=0;j<p;j++) {
        for(int i=0;i<m;i++) {
	    //std::cout << "i = " << i << ", j = " << j << std::endl;
	    ////std::cout << "arma::span(p-1-j,n-1-j)" << arma::span(p-1-j,n-1-j) << std:endl;
	    //std::cout << "x(arma::span(p-1-j,n-1-j),i)" << x(arma::span(p-1-j,n-1-j),i) << std::endl;
            y.col(i+j*m) = x(arma::span(p-1-j,n-1-j),i);
        }
    }
    //std::cout << "y: " << std::endl;
    //std::cout << "END embed_mat" << std::endl;
    return y;
} //matrix embed

arma::vec ridge (const arma::vec y,const arma::mat x,arma::mat XX) {

    //Dimensions and initialization
    int n = x.n_cols; arma::vec coef = arma::zeros(n);
    int t = y.n_elem;
    int tau = floor(double(y.n_elem*2/3)); //sample size and cutoff

    //Start cross-validation for lambda
    double tol = 1e-10; //minimum condition number allowed
    arma::vec y_train = y.rows(0,tau-1); arma::vec y_test = y.rows(tau,t-1);
    arma::mat x_train = x.rows(0,tau-1); arma::mat x_test = x.rows(tau,t-1);
    arma::mat xx = x_train.t() * x_train; arma::vec xy = x_train.t() * y_train;
    arma::vec s = svd(xx); //singular values of x'x
    arma::vec a; a << 0.0 << arma::endr << (max(s)*tol - min(s))/(1-tol) << arma::endr; //vector with (0,LB)
    double lambda_min = max(a); //minimum penalty to ensure good conditioning
    arma::vec lambdas = arma::zeros(6); //initial lambdas
    arma::vec CVs = arma::zeros(6); //store cross-validation results
    arma::vec beta = arma::zeros(n);
    for(int i = 0;i<6;i++) {
        double lambda_tmp = pow(10.,i-1) + lambda_min; //always ensures good conditioning
        lambdas(i) = lambda_tmp; //store lambda
        beta = arma::inv_sympd( xx + lambda_tmp*arma::eye(n,n)) * xy; //Ridge estimator, improve later
        double msfe = sum(pow(y_test - x_test*beta,2));
        CVs(i) = msfe;
    }

    //Choose best lambda and obtain ridge estimator
    arma::uword i_min = CVs.index_min(); //index for best lambda
    double lambda_opt = lambdas(i_min);
    coef = arma::inv_sympd(XX + lambda_opt*arma::eye(n,n)) * x.t() * y; //Ridge estimator, improve later
    return coef;
}

// [[Rcpp::export]]
Rcpp::List cecm (arma::vec y,arma::mat x,const int p,bool ADL) {

    //Create CECM specification
    arma::mat z = arma::join_horiz(y,x); //combined data
    arma::mat z_d = diff_mat(z); //differences
    arma::mat z_dl = embed_mat(z_d,p+1); //lagged differences
    arma::vec y_d = z_dl.col(0); //differenced y
    arma::mat z_l = z.rows(p,z.n_rows-2); //lagged levels
    arma::mat w = z_dl.cols(1,z_dl.n_cols-1); //differences in CECM
    arma::mat v;
    if(ADL) {
        v = w; //consider diffs only
    }else {
        v = arma::join_horiz(z_l,w); //combined lagged lvls and diffs
    }

    //Collect output in list
    Rcpp::List ret;
    ret["v"] = v;
    ret["w"] = w;
    ret["y_d"] = y_d;
    ret["z_l"] = z_l;
    return ret;
}

// [[Rcpp::export]]
Rcpp::List specs_rcpp (const arma::vec y,const arma::mat x,const int p,
                       std::string deterministics,bool ADL,arma::vec weights,
                       const double k_delta, const double k_pi,
                       arma::vec lambda_g, arma::vec lambda_i,
                       const double thresh,const double max_iter_delta,
                       const double max_iter_pi,const double max_iter_gamma) {
    //std::cout << "In specs_rcpp..." << std::endl;
    
    std::cout << "Parameters:" << std::endl;
    //std::cout << "y: " << y << std::endl;
    //std::cout << "x: " << x << std::endl;
    //std::cout << "p: " << p << std::endl;
    //std::cout << "deterministics: " << deterministics << std::endl;
    //std::cout << "ADL: " << ADL << std::endl;
    std::cout << "weights: " << weights << std::endl;
    //std::cout << "k_delta: " << k_delta << std::endl;
    //std::cout << "k_pi: " << k_pi << std::endl;
    //std::cout << "lambda_g: " << lambda_g << std::endl;
    //std::cout << "lambda_i: " << lambda_i << std::endl;
    //std::cout << "thresh: " << thresh << std::endl;
    //std::cout << "max_iter_delta: " << max_iter_delta << std::endl;
    //std::cout << "max_iter_pi: " << max_iter_pi << std::endl;
    //std::cout << "max_iter_gamma: " << max_iter_gamma << std::endl;
    //std::cout << "(end of parameters)" << std::endl;

    //Create CECM specification
    //std::cout << "CECM specification:" << std::endl;
    arma::mat z = arma::join_horiz(y,x); //combined data
    arma::mat z_d = diff_mat(z); //differences
    arma::mat z_dl = embed_mat(z_d,p+1); //lagged differences
    arma::vec y_d = z_dl.col(0); //differenced y
    arma::mat z_l = z.rows(p,z.n_rows-2); //lagged levels
    arma::mat w = z_dl.cols(1,z_dl.n_cols-1); //differences in CECM
    arma::mat v;
    std::cout << "z: " << z << std::endl;
    std::cout << "z_d: " << z_d << std::endl;
    std::cout << "z_dl: " << z_dl << std::endl;
    std::cout << "y_d: " << y_d << std::endl;
    std::cout << "z_l: " << z_l << std::endl;
    std::cout << "w: " << w << std::endl;
    if(ADL) {
        std::cout << "ADL" << std::endl;
        v = w; //consider diffs only
    } else {
        std::cout << "No ADL" << std::endl;
        v = arma::join_horiz(z_l,w); //combined lagged lvls and diffs
    }
    int n = z.n_cols; int m = v.n_cols; int t = y_d.n_elem;
    std::cout << "v: " << v << std::endl;
    std::cout << "n: " << n << std::endl;
    std::cout << "m: " << m << std::endl;
    std::cout << "t: " << t << std::endl;
    std::cout << "(End of CECM specification.)" << std::endl;

    //regress out deterministics
    //std::cout << "Regressing out deterministics" << std::endl;
    arma::vec y_d_old; arma::mat v_old, D, DDD;
    if (deterministics == "constant") {
	std::cout << "We are in this case: constant" << std::endl;
        arma::mat M = arma::eye(t,t) - arma::mat(t,t,arma::fill::ones)/t;
        y_d_old = y_d; v_old = v;
        y_d = M*y_d; v = M*v;
        D = join_horiz(arma::mat(t,1,arma::fill::ones),arma::mat(t,1,arma::fill::zeros));
	std::cout << "M: " << M << std::endl;
	std::cout << "y_d_old: " << y_d_old << std::endl;
	std::cout << "y_d: " << y_d << std::endl;
	std::cout << "D: " << D << std::endl;
    } else if (deterministics == "trend") {
	std::cout << "We are in this case: trend" << std::endl;
        D = arma::linspace(1,t,t);
        arma::mat M = arma::eye(t,t) - D*D.t()/arma::as_scalar(sum(pow(D,2)));
        y_d_old = y_d; v_old = v;
        y_d = M*y_d; v = M*v;
        D = join_horiz(arma::mat(t,1,arma::fill::zeros),D);
	std::cout << "D: " << D << std::endl;
	std::cout << "M: " << M << std::endl;
	std::cout << "y_d_old: " << y_d_old << std::endl;
	std::cout << "y_d: " << y_d << std::endl;
    } else if (deterministics == "both"){
	std::cout << "We are in this case: both" << std::endl;
        D = join_horiz(arma::ones(t),arma::linspace(1,t,t));
        DDD = arma::inv_sympd(D.t() * D)*D.t();
        arma::mat M = arma::eye(t,t) - D*DDD;
        y_d_old = y_d; v_old = v;
        y_d = M*y_d; v = M*v;
	std::cout << "D: " << D << std::endl;
	std::cout << "DDD: " << DDD << std::endl;
	std::cout << "M: " << M << std::endl;
	std::cout << "y_d_old: " << y_d_old << std::endl;
	std::cout << "v_old: " << v_old << std::endl;
	std::cout << "y_d: " << y_d << std::endl;
	std::cout << "v: " << v << std::endl;
    } else {
	std::cout << "We are in this case: else" << std::endl;
        D = arma::mat(t,2,arma::fill::zeros);
	std::cout << "D: " << D << std::endl;
    }
    std::cout << "(Done regressing out deterministics.)" << std::endl;

    //Obtain crossproducts
    std::cout << "Obtaining crossproducts" << std::endl;
    arma::mat VV = v.t() * v; //crossproduct of V
    arma::vec vv = VV.diag(); //Diagonal elements of V'V
    arma::vec vy = v.t()*y_d/t; //Used for lambda seq and soft-thresholding
    std::cout << "VV: " << VV << std::endl;
    std::cout << "vv: " << vv << std::endl;
    std::cout << "vy: " << vy << std::endl;
    std::cout << "(Done obtaining crossproducts.)" << std::endl;

    //Obtain weights (if necessary)
    std::cout << "Obtain weights (if necessary)" << std::endl;
    std::cout << "weights before: " << weights << std::endl;
    arma::vec gamma0;
    if(weights(0)==-1){
	std::cout << "In case 1" << std::endl;
        gamma0 = arma::inv_sympd(VV) * v.t() * y_d; //OLS estimator
	std::cout << "gamma0: " << gamma0 << std::endl;
        if(!ADL){
	    std::cout << "In case 1.1" << std::endl; 
            weights = join_cols(pow(abs(gamma0.rows(0,n-1)),-k_delta),pow(abs(gamma0.rows(n,m-1)),-k_pi));
        }else{
	    std::cout << "In case 1.2" << std::endl;
            weights = pow(abs(gamma0),-k_pi);
        }
    }else if (weights(0) == -2) {
	std::cout << "In case 2" << std::endl;
        gamma0 = ridge(y_d,v,VV); //ridge estimator (change function later with svd check)
        std::cout << "gamma0: " << gamma0 << std::endl;
        if(!ADL){
	    std::cout << "In case 2.1" << std::endl;
            weights = join_cols(pow(abs(gamma0.rows(0,n-1)),-k_delta),pow(abs(gamma0.rows(n,m-1)),-k_pi));
        }else{
	    std::cout << "In case 2.2" << std::endl;
            weights = pow(abs(gamma0),-k_pi);
        }
    }
    std::cout << "weights: " << weights << std::endl;
    std::cout << "(Done obtaining weights.)" << std::endl;

    //Set group penalty
    std::cout << "Setting group penalty" << std::endl;
    if (lambda_g(0) == -1){
	std::cout << "In case 1" << std::endl;
        double lambda_gmax = sqrt(as_scalar(sum(pow(vy.rows(0,n-1),2)))); //maximum group penalty
        double lambda_gmin = 1e-4*lambda_gmax; //minimum group penalty
        std::cout << "lambda_gmax: " << lambda_gmax << std::endl;
	std::cout << "lambda_gmin: " << lambda_gmin << std::endl;
        lambda_g = arma::zeros(10); //initialize group penalties
        lambda_g.rows(0,8) = exp(arma::linspace(log(lambda_gmax),log(lambda_gmin),9)); //fill group penalties
    }
    std::cout << "lambda_g: " << lambda_g << std::endl;
    std::cout << "(Done setting group penalty.)" << std::endl;

    //Set individual penalty
    std::cout << "Setting individual penalty" << std::endl;
    if(lambda_i(0) == -1) {
	std::cout << "In case 1" << std::endl;
        std::cout << "weights: " << weights << std::endl;
	std::cout << "pow(weights,-1): " << pow(weights,-1) << std::endl;
	std::cout << "vy: " << vy << std::endl;
        std::cout << "vy%pow(weights,-1): " << vy%pow(weights,-1) << std::endl;
        std::cout << "abs(vy%pow(weights,-1)): " << abs(vy%pow(weights,-1)) << std::endl;
        std::cout << "max(abs(vy%pow(weights,-1))): " << max(abs(vy%pow(weights,-1))) << std::endl;
        double lambda_imax = arma::as_scalar(max(abs(vy%pow(weights,-1)))); //maximum individual penalty
        double lambda_imin = 1e-4*lambda_imax; //minimum individual penalty
	std::cout << "lambda_imax: " << lambda_imax << std::endl;
	std::cout << "lambda_imin: " << lambda_imin << std::endl;
        lambda_i = exp(arma::linspace(log(lambda_imax),log(lambda_imin),100)); //individual penalties
    }
    int n_i = lambda_i.n_elem; int n_g = lambda_g.n_elem;
    std::cout << "lambda_i: " << lambda_i << std::endl;
    std::cout << "n_i: " << n_i << std::endl;
    std::cout << "n_g: " << n_g << std::endl;
    std::cout << "(Done setting individual penalty)" << std::endl;

    //Perform SGL algorithm
    //std::cout << "Performing SGL algorithm" << std::endl;
    arma::vec gamma = arma::zeros(m);//initial
    arma::vec gamma_old;//coefficients vectors
    arma::vec theta, theta_dif, delta_u, xr_st; //initialize vectors for delta updates
    if((any(lambda_g)>0) && !ADL){
	//std::cout << "In case 1" << std::endl;
        theta = theta_dif = delta_u = xr_st = arma::zeros(n);
        std::cout << "theta: " << theta << std::endl;
        std::cout << "theta_dif: " << theta_dif << std::endl;
        std::cout << "delta_u: " << delta_u << std::endl;
        std::cout << "xr_st: " << xr_st << std::endl;
    }
    arma::mat gammas(m,n_i*n_g,arma::fill::zeros); //coefficient matrix
    arma::vec weights_vv = weights % pow(vv,-1);//weights divided by vv
    arma::mat v_vv = v*diagmat(pow(vv,-1));//columns of v divided by vv (standardized)
    std::cout << "weights_vv: " << weights_vv << std::endl;
    std::cout << "v_vv: " << v_vv << std::endl;
    std::cout << "(Done performing SGL algorithm)" << std::endl;

    //Double loop over penalties
    std::cout << "Double loop over penalties" << std::endl;
    for(int g = 0;g<n_g;g++) {
        double lambda_gtmp = lambda_g(g);
        std::cout << "Outer loop, g: " << g << ", lambda_gtmp: " << lambda_gtmp << std::endl;
        if(lambda_gtmp > 0) {
            std::cout << "ldmbda_gtmp > 0" << std::endl;
            double l2; //initialize l2 norm
            for(int i=0;i<n_i;i++) {
                arma::vec lambda_itmp = lambda_i(i)*weights; //Create weighted individual penalties
                std::cout << "Inner loop, i: " << i << ", lambda_itmp: " << lambda_itmp << std::endl;

		// TODO !!!!! Added
		// if (i > 3) break;

                //Iterate over gamma for fixed lambda pair
                double dist_gamma = thresh+1;
                int count_gamma = 0;
                while(dist_gamma > thresh) {
                    Rcpp::checkUserInterrupt();
                    count_gamma++;
                    if(count_gamma == max_iter_gamma) {
                        Rcout << "Warning: gamma did not converge in " << max_iter_gamma <<
                            " steps with (lambda_g,lambda_i) = (" << lambda_gtmp << "," << lambda_i(i) <<
                                ")" << std::endl;
                        break;
                    }
                    gamma_old = gamma; //recenter gamma
                    arma::vec r_delta = y_d - w*gamma.rows(n,m-1); //
                    arma::vec xr = z_l.t()*r_delta/t;
                    for(int j=0;j<n;j++) {
                        xr_st(j) = st(xr(j),lambda_itmp(j));//Get soft-thresholded xr for delta group
                        std::cout <<"!!! xr_st[" << j << "]: " << xr_st(j) << std::endl;
                    }

                    //Loop for delta
                    if(as_scalar(sqrt(sum(pow(xr_st,2)))) > lambda_gtmp) {
                        double dist_delta = thresh+1;
                        int count = 0;
                        while(dist_delta > thresh) {
                            count = count + 1;
                            if(count == max_iter_delta) {
                                Rcout << "Warning: delta did not converge in " << max_iter_delta <<
                                    " steps with (lambda_g,lambda_i) = (" << lambda_gtmp << "," << lambda_i(i) <<
                                        ")" << std::endl;
                                break;
                            }
                            gamma_old.rows(0,n-1) = gamma.rows(0,n-1);//Update delta
                            arma::vec grad = (-z_l.t()*(r_delta-z_l*gamma_old.rows(0,n-1)))/t; //gradient of the L2-part

                            //Optimize step size
                            bool s_cond = true;
                            double s = 1; //initial step size
                            double s_lb = arma::as_scalar(sum(pow(r_delta - z_l*gamma_old.rows(0,n-1),2))); //part of step update
                            int l = 0; //iteration counter
                            while(s_cond) {
                                l++;
                                s = 0.5*s;
                                for(int j=0;j<n;j++) {
                                    delta_u(j) = st(gamma_old(j)-s*grad(j),s*lambda_itmp(j));//soft-threshold update
                                }
                                theta = (std::max(0.0,1-s*lambda_gtmp/as_scalar(sqrt(sum(pow(delta_u,2))))))*delta_u;//new delta
                                theta_dif = theta - gamma_old.rows(0,n-1); //interim dif
                                s_cond = arma::as_scalar(sum(pow(r_delta - z_l*theta,2))) > s_lb +
                                    arma::as_scalar(grad.t()*theta_dif) + arma::as_scalar(sum(pow(theta_dif,2))); //stopping condition for step size
                            }

                            //Update delta and check convergence
                            gamma.rows(0,n-1) = gamma_old.rows(0,n-1) + l*(theta-gamma_old.rows(0,n-1))/(l+3); //Nesterov Update
                            dist_delta = sqrt(sum(pow(gamma.rows(0,n-1)-gamma_old.rows(0,n-1),2))); //update distance
                        }
                    } else {
                        gamma.rows(0,n-1).fill(0); //set delta to zero
                    }

                    //Update pi via coordinate descent
                    arma::vec r = y_d - v*gamma;  arma::vec r_j;//residuals
                    arma::vec lambda_itmp = t*lambda_i(i)*weights_vv.rows(n,m-1); //t*lambda*weight/vv (see above)
                    double dist_pi = thresh + 1;
                    int count = 0;
                    while(dist_pi>thresh){
                        count = count + 1;
                        if(count == max_iter_pi) {
                            Rcout << "Warning: pi did not converge in " << max_iter_pi <<
                                " steps with (lambda_g,lambda_i) = (" << lambda_gtmp << "," << lambda_i(i) <<
                                    ")" << std::endl;
                            break;
                        }
                        gamma_old.rows(n,m-1) = gamma.rows(n,m-1); //recenter gamma
                        for(int j=n;j<m;j++){
                            if((count>1 && gamma(j)!=0) || count==1) {
                                r_j = r + v.col(j)*gamma(j); //Remove effect of gamma_j
                                gamma(j) = st(as_scalar(v_vv.col(j).t()*r_j),lambda_itmp(j-n)); //gamma_j update
                                r = r_j - v.col(j)*gamma(j); //Add back effect of gamma_j
                            }
                        }
                        dist_pi = sqrt(sum(pow(gamma.rows(n,m-1)-gamma_old.rows(n,m-1),2)));//update distance
                    }

                    //Update gamma and check convergence
                    arma::uvec ind = find(gamma_old); //non-zero elements
                    l2 = sqrt(sum(pow(gamma_old(ind),2)));
                    if(l2>0){
                        dist_gamma = sqrt(sum(pow(gamma(ind)-gamma_old(ind),2)))/l2;//update distance
                    } else {
                        dist_gamma = sqrt(sum(pow(gamma(ind),2)));
                    }
                } //closes while loop for gamma

                gammas.col(i+g*n_i) = gamma; //Store new gamma in gammas
                std::cout << "NEW GAMMA: " << gamma << std::endl; 
            } //closes loop over individual penalties
        } else {
            //Full coordinate descent
            std::cout << "*** Full coordinate descent ***" << std::endl;
            arma::vec r = y_d; arma::vec r_j;
            for(int i=0;i<n_i;i++) {
                arma::vec lambda_itmp = t*lambda_i(i)*weights_vv; //no more need to calculate lambda*weight/vv (see above)

                std::cout << "Inner loop, i: " << i << ", lambda_itmp: " << lambda_itmp << std::endl;

		// TODO !!!!! Added
		// if (i > 3) break;

                double dist = thresh+1;
                int count = 0;
                while(dist>thresh) {
                    Rcpp::checkUserInterrupt();
                    count++;
                    if(count == max_iter_gamma) {
                        Rcout << "Warning: gamma did not converge in " << max_iter_delta <<
                            " steps with (lambda_g,lambda_i) = (" << 0 << "," << lambda_i(i) <<
                                ")" << std::endl;
                        break;
                    }
                    gamma_old = gamma; //recenter gamma_old
                    for(int j=0;j<m;j++){
                        if((count>1 && gamma(j)!=0) || count==1) {

                            //active set updates
                            r_j = r + v.col(j)*gamma(j); //Remove effect of gamma_j
                            gamma(j) = st(as_scalar(v_vv.col(j).t()*r_j),lambda_itmp(j)); //gamma_j update
                            r = r_j - v.col(j)*gamma(j); //Add back effect of gamma_j
                        }
                    }

                    //Update gamma and check convergence
                    arma::uvec ind = find(gamma_old); //non-zero elements
                    double l2 = sqrt(sum(pow(gamma_old(ind),2)));
                    if(l2>0){
                        dist = sqrt(sum(pow(gamma(ind)-gamma_old(ind),2)))/l2;//update distance
                    } else {
                        dist = sqrt(sum(pow(gamma(ind),2)));
                    }

                }
                gammas.col(i+g*n_i) = gamma; //Store new gamma in gammas = gamma; //Store new gamma in gammas
            }
        }
        gamma = arma::zeros(m); //reset gamma for new group penalty

        // !!!!!!! TODO Added:
        // break;
    } //closes loop over group penalties
    std::cout << "(Done loop over penalties)" << std::endl;

    //Calculate deterministic coefficients
    // std::cout << "Calculate deterministic coefficients" << std::endl;
    arma::mat det(2,n_g*n_i,arma::fill::zeros); int nn = n_g*n_i;
    if (deterministics == "constant") {
        for(int i=0;i<nn;i++) {
            det.submat(0,i,0,i) = sum(y_d_old - v_old*gammas.col(i))/t;
        }
        arma::mat tau(1,n_g*n_i,arma::fill::zeros);
        det.row(1) = tau;
    } else if (deterministics == "trend"){
        double trend_sq = arma::as_scalar(sum(pow(D,2)));
        for(int i=0;i<nn;i++) {
            det(2,i) = arma::as_scalar(D.t()*(y_d_old - v_old*gammas.col(i))/trend_sq);
        }
    }else if (deterministics == "both"){
        for(int i=0;i<nn;i++) {
            det.col(i) = DDD*(y_d_old - v_old*gammas.col(i));
        }
    }
    // std::cout << "(Done calculating deterministic coefficients)" << std::endl;

    //Collect output in list
    //std::cout << "Collect output in list" << std::endl;
    Rcpp::List ret ;
    ret["gammas"] = gammas;
    ret["thetas"] = det;
    ret["lambda_i"] = lambda_i;
    ret["lambda_g"] = lambda_g;
    ret["weights"] = weights;
    if (deterministics == "none") {
        ret["y_d"] = ret["My_d" ] = y_d;
        ret["v"] = ret["Mv"] = v;
    }else {
        ret["y_d"] = y_d; ret["My_d" ] = y_d_old;
        ret["v"] = v; ret["Mv"] = v_old;
    }
    ret["D"] = D;
    //std::cout << "(Collected output in list)" << std::endl;

    //std::cout << "ret[gammas]: " << gammas << std::endl;
    //std::cout << "ret[thetas]: " << det << std::endl;
    //std::cout << "ret[lambda_i]: " << lambda_i << std::endl;
    //std::cout << "ret[lambda_g]: " << lambda_g << std::endl;
    //std::cout << "ret[weights]: " << weights << std::endl;
    if (deterministics == "none") {
        //std::cout << "ret[y_d]: " << y_d << std::endl;
        //std::cout << "ret[My_d]: " << y_d << std::endl;
        //std::cout << "ret[v]: " << v << std::endl;
        //std::cout << "ret[Mv]: " << v << std::endl;
    } else {
        //std::cout << "ret[y_d]: " << y_d << std::endl;
        //std::cout << "ret[My_d]: " << y_d_old << std::endl;
        //std::cout << "ret[v]: " << v << std::endl;
        //std::cout << "ret[Mv]: " << v_old << std::endl;
    }
    //std::cout << "ret[D]: " << D << std::endl;

    return ret;
}

// [[Rcpp::export]]
Rcpp::List specs_tr_rcpp (arma::vec y_d,arma::mat z_l,arma::mat w,
                          std::string deterministics,bool ADL,
                          arma::vec weights,const double k_delta, const double k_pi,
                          arma::vec lambda_g, arma::vec lambda_i,
                          const double thresh,const double max_iter_delta,
                          const double max_iter_pi,const double max_iter_gamma) {

    //Set v and dimensions
    arma::mat v;
    if(ADL) {
        v = w; //consider diffs only
    }else {
        v = arma::join_horiz(z_l,w); //combined lagged lvls and diffs
    }
    int n = z_l.n_cols; int m = v.n_cols; int t = y_d.n_elem;

    //regress out deterministics
    arma::vec y_d_old; arma::mat v_old, D, DDD;
    if (deterministics == "constant") {
        arma::mat M = arma::eye(t,t) - arma::mat(t,t,arma::fill::ones)/t;
        y_d_old = y_d; v_old = v;
        y_d = M*y_d; v = M*v;
        D = join_horiz(arma::mat(t,1,arma::fill::ones),arma::mat(t,1,arma::fill::zeros));
    } else if (deterministics == "trend") {
        D = arma::linspace(1,t,t);
        arma::mat M = arma::eye(t,t) - D*D.t()/arma::as_scalar(sum(pow(D,2)));
        y_d_old = y_d; v_old = v;
        y_d = M*y_d; v = M*v;
        D = join_horiz(arma::mat(t,1,arma::fill::zeros),D);
    } else if (deterministics == "both"){
        D = join_horiz(arma::ones(t),arma::linspace(1,t,t));
        DDD = arma::inv_sympd(D.t() * D)*D.t();
        arma::mat M = arma::eye(t,t) - D*DDD;
        y_d_old = y_d; v_old = v;
        y_d = M*y_d; v = M*v;
    } else {
        D = arma::mat(t,2,arma::fill::zeros);
    }

    //Obtain crossproducts
    arma::mat VV = v.t() * v; //crossproduct of V
    arma::vec vv = VV.diag(); //Diagonal elements of V'V
    arma::vec vy = v.t()*y_d/t; //Used for lambda seq and soft-thresholding

    //Obtain weights (if necessary)
    arma::vec gamma0;
    if(weights(0)==-1){
        gamma0 = arma::inv_sympd(VV) * v.t() * y_d; //OLS estimator
        if(!ADL){
            weights = join_cols(pow(abs(gamma0.rows(0,n-1)),-k_delta),pow(abs(gamma0.rows(n,m-1)),-k_pi));
        }else{
            weights = pow(abs(gamma0),-k_pi);
        }
    }else if (weights(0) == -2) {
        gamma0 = ridge(y_d,v,VV); //ridge estimator (change function later with svd check)
        if(!ADL){
            weights = join_cols(pow(abs(gamma0.rows(0,n-1)),-k_delta),pow(abs(gamma0.rows(n,m-1)),-k_pi));
        }else{
            weights = pow(abs(gamma0),-k_pi);
        }
    }

    //Set group penalty
    if (lambda_g(0) == -1){
        double lambda_gmax = sqrt(as_scalar(sum(pow(vy.rows(0,n-1),2)))); //maximum group penalty
        double lambda_gmin = 1e-4*lambda_gmax; //minimum group penalty
        lambda_g = arma::zeros(10); //initialize group penalties
        lambda_g.rows(0,8) = exp(arma::linspace(log(lambda_gmax),log(lambda_gmin),9)); //fill group penalties
    }

    //Set individual penalty
    if(lambda_i(0) == -1) {
        double lambda_imax = arma::as_scalar(max(abs(vy%pow(weights,-1)))); //maximum individual penalty
        double lambda_imin = 1e-4*lambda_imax; //minimum individual penalty
        lambda_i = exp(arma::linspace(log(lambda_imax),log(lambda_imin),100)); //individual penalties
    }
    int n_i = lambda_i.n_elem; int n_g = lambda_g.n_elem;

    //Perform SGL algorithm
    arma::vec gamma = arma::zeros(m);//initial
    arma::vec gamma_old;//coefficients vectors
    arma::vec theta, theta_dif, delta_u, xr_st; //initialize vectors for delta updates
    if((any(lambda_g)>0) && !ADL){
        theta = theta_dif = delta_u = xr_st = arma::zeros(n);
    }
    arma::mat gammas(m,n_i*n_g,arma::fill::zeros); //coefficient matrix
    arma::vec weights_vv = weights % pow(vv,-1);//weights divided by vv
    arma::mat v_vv = v*diagmat(pow(vv,-1));//columns of v divided by vv (standardized)

    //Double loop over penalties
    for(int g = 0;g<n_g;g++) {
        double lambda_gtmp = lambda_g(g);
        if(lambda_gtmp > 0) {
            double l2; //initialize l2 norm
            for(int i=0;i<n_i;i++) {
                arma::vec lambda_itmp = lambda_i(i)*weights; //Create weighted individual penalties

                //Iterate over gamma for fixed lambda pair
                double dist_gamma = thresh+1;
                int count_gamma = 0;
                while(dist_gamma > thresh) {
                    Rcpp::checkUserInterrupt();
                    count_gamma++;
                    if(count_gamma == max_iter_gamma) {
                        Rcout << "Warning: gamma did not converge in " << max_iter_gamma <<
                            " steps with (lambda_g,lambda_i) = (" << lambda_gtmp << "," << lambda_i(i) <<
                                ")" << std::endl;
                        break;
                    }
                    gamma_old = gamma; //recenter gamma
                    arma::vec r_delta = y_d - w*gamma.rows(n,m-1); //
                    arma::vec xr = z_l.t()*r_delta/t;
                    for(int j=0;j<n;j++) {
                        xr_st(j) = st(xr(j),lambda_itmp(j));//Get soft-thresholded xr for delta group
                    }

                    //Loop for delta
                    if(as_scalar(sqrt(sum(pow(xr_st,2)))) > lambda_gtmp) {
                        double dist_delta = thresh+1;
                        int count = 0;
                        while(dist_delta > thresh) {
                            count = count + 1;
                            if(count == max_iter_delta) {
                                Rcout << "Warning: delta did not converge in " << max_iter_delta <<
                                    " steps with (lambda_g,lambda_i) = (" << lambda_gtmp << "," << lambda_i(i) <<
                                        ")" << std::endl;
                                break;
                            }
                            gamma_old.rows(0,n-1) = gamma.rows(0,n-1);//Update delta
                            arma::vec grad = (-z_l.t()*(r_delta-z_l*gamma_old.rows(0,n-1)))/t; //gradient of the L2-part

                            //Optimize step size
                            bool s_cond = true;
                            double s = 1; //initial step size
                            double s_lb = arma::as_scalar(sum(pow(r_delta - z_l*gamma_old.rows(0,n-1),2))); //part of step update
                            int l = 0; //iteration counter
                            while(s_cond) {
                                l++;
                                s = 0.5*s;
                                for(int j=0;j<n;j++) {
                                    delta_u(j) = st(gamma_old(j)-s*grad(j),s*lambda_itmp(j));//soft-threshold update
                                }
                                theta = (std::max(0.0,1-s*lambda_gtmp/as_scalar(sqrt(sum(pow(delta_u,2))))))*delta_u;//new delta
                                theta_dif = theta - gamma_old.rows(0,n-1); //interim dif
                                s_cond = arma::as_scalar(sum(pow(r_delta - z_l*theta,2))) > s_lb +
                                    arma::as_scalar(grad.t()*theta_dif) + arma::as_scalar(sum(pow(theta_dif,2))); //stopping condition for step size
                            }

                            //Update delta and check convergence
                            gamma.rows(0,n-1) = gamma_old.rows(0,n-1) + l*(theta-gamma_old.rows(0,n-1))/(l+3); //Nesterov Update
                            dist_delta = sqrt(sum(pow(gamma.rows(0,n-1)-gamma_old.rows(0,n-1),2))); //update distance
                        }
                    } else {
                        gamma.rows(0,n-1).fill(0); //set delta to zero
                    }

                    //Update pi via coordinate descent
                    arma::vec r = y_d - v*gamma;  arma::vec r_j;//residuals
                    arma::vec lambda_itmp = t*lambda_i(i)*weights_vv; //no more need to calculate lambda*weight/vv
                    double dist_pi = thresh + 1;
                    int count = 0;
                    while(dist_pi>thresh){
                        count = count + 1;
                        if(count == max_iter_pi) {
                            Rcout << "Warning: pi did not converge in " << max_iter_pi <<
                                " steps with (lambda_g,lambda_i) = (" << lambda_gtmp << "," << lambda_i(i) <<
                                    ")" << std::endl;
                            break;
                        }
                        gamma_old.rows(n,m-1) = gamma.rows(n,m-1); //recenter gamma
                        for(int j=n;j<m;j++){
                            if((count>1 && gamma(j)!=0) || count==1) {
                                r_j = r + v.col(j)*gamma(j); //Remove effect of gamma_j
                                gamma(j) = st(as_scalar(v_vv.col(j).t()*r_j),lambda_itmp(j-n)); //gamma_j update
                                r = r_j - v.col(j)*gamma(j); //Add back effect of gamma_j
                            }
                        }
                        dist_pi = sqrt(sum(pow(gamma.rows(n,m-1)-gamma_old.rows(n,m-1),2)));//update distance
                    }

                    //Update gamma and check convergence
                    arma::uvec ind = find(gamma_old); //non-zero elements
                    l2 = sqrt(sum(pow(gamma_old(ind),2)));
                    if(l2>0){
                        dist_gamma = sqrt(sum(pow(gamma(ind)-gamma_old(ind),2)))/l2;//update distance
                    } else {
                        dist_gamma = sqrt(sum(pow(gamma(ind),2)));
                    }
                } //closes while loop for gamma

                gammas.col(i+g*n_i) = gamma; //Store new gamma in gammas
            } //closes loop over individual penalties
        } else {
            //Full coordinate descent
            arma::vec r = y_d; arma::vec r_j;
            for(int i=0;i<n_i;i++) {
                arma::vec lambda_itmp = t*lambda_i(i)*weights_vv; //no more need to calculate lambda*weight/vv (see above)
                double dist = thresh+1;
                int count = 0;
                while(dist>thresh) {
                    Rcpp::checkUserInterrupt();
                    count++;
                    if(count == max_iter_gamma) {
                        Rcout << "Warning: gamma did not converge in " << max_iter_delta <<
                            " steps with (lambda_g,lambda_i) = (" << 0 << "," << lambda_i(i) <<
                                ")" << std::endl;
                        break;
                    }
                    gamma_old = gamma; //recenter gamma_old
                    for(int j=0;j<m;j++){
                        if((count>1 && gamma(j)!=0) || count==1) {

                            //active set updates
                            r_j = r + v.col(j)*gamma(j); //Remove effect of gamma_j
                            gamma(j) = st(as_scalar(v_vv.col(j).t()*r_j),lambda_itmp(j)); //gamma_j update
                            r = r_j - v.col(j)*gamma(j); //Add back effect of gamma_j
                        }
                    }

                    //Update gamma and check convergence
                    arma::uvec ind = find(gamma_old); //non-zero elements
                    double l2 = sqrt(sum(pow(gamma_old(ind),2)));
                    if(l2>0){
                        dist = sqrt(sum(pow(gamma(ind)-gamma_old(ind),2)))/l2;//update distance
                    } else {
                        dist = sqrt(sum(pow(gamma(ind),2)));
                    }

                }
                gammas.col(i+g*n_i) = gamma; //Store new gamma in gammas = gamma; //Store new gamma in gammas
            }
        }
        gamma = arma::zeros(m); //reset gamma for new group penalty
    } //closes loop over group penalties

    //Calculate deterministic coefficients
    arma::mat det(2,n_g*n_i,arma::fill::zeros); int nn = n_g*n_i;
    if (deterministics == "constant") {
        for(int i=0;i<nn;i++) {
            det.submat(0,i,0,i) = sum(y_d_old - v_old*gammas.col(i))/t;
        }
        arma::mat tau(1,n_g*n_i,arma::fill::zeros);
        det.row(1) = tau;
    } else if (deterministics == "trend"){
        double trend_sq = arma::as_scalar(sum(pow(D,2)));
        for(int i=0;i<nn;i++) {
            det(2,i) = arma::as_scalar(D.t()*(y_d_old - v_old*gammas.col(i))/trend_sq);
        }
    }else if (deterministics == "both"){
        for(int i=0;i<nn;i++) {
            det.col(i) = DDD*(y_d_old - v_old*gammas.col(i));
        }
    }

    //Collect output in list
    Rcpp::List ret ;
    ret["gammas"] = gammas;
    ret["thetas"] = det;
    ret["lambda_i"] = lambda_i;
    ret["lambda_g"] = lambda_g;
    ret["weights"] = weights;
    if (deterministics == "none") {
        ret["y_d"] = ret["My_d" ] = y_d;
        ret["v"] = ret["Mv"] = v;
    }else {
        ret["y_d"] = y_d_old; ret["My_d" ] = y_d;
        ret["v"] = v_old; ret["Mv"] = v;
    }
    ret["D"] = D;
    return ret;
}
