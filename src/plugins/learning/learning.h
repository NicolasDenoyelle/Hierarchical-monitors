/**************************************************************** Learning ********************************************************************/
struct perceptron;

/**
 * Create a new perceptron to fit a function.
 * @param n: The number of features of the perceptron.
 * @return a new perceptron with feature parameters initialized to 1 and, learning rate initialize to ALPHA and features not initialized.
 **/
struct perceptron * new_perceptron(const int n, const double max, const double min);

/**
 * Delete a perceptron
 * @param p: The perceptron to delete
 **/
void delete_perceptron(struct perceptron * p);

/**
 * Make perceptron fit Y from features samples X using gradiant descent iterations.
 * @param p: The perceptron to update.
 * @param X: the input scaled features. Must be a m*n (m row samples of n column features) matrix.
 * @param Y: A vector of length m of output samples. Changed as a side effect.
 * @param m: The number of samples.
 * @return The cost function of previous Theta.
 * The perceptron's features parameter are updated using gradiant descent on mean square cost function.
 **/
double perceptron_fit_by_gradiant_descent(struct perceptron * p, const double * X, double * Y, const int m);

/**
 * Make perceptron fit Y from features samples X using dgelsd to minimize least square sum.
 * @param p: The perceptron to update.
 * @param X: the input features. Must be a m*n (m row samples of n column features) matrix. (Overwritten)
 * @param Y: A vector of length m of output samples. (Overwritten)
 * @param m: The number of samples.
 * @return -1 if the function dgelsd failed, else 0.
 * The perceptron's features parameter are updated using dgelsd.
 **/
int perceptron_fit_by_normal_equation(struct perceptron * p, double * X, double * Y, const int m);
    
/**
 * Output prediction based on input features value.
 * @param p: the perceptron to use for learning.
 * @param X: the input features for prediction.
 * @return A the predicted value.
 **/
double perceptron_output(const struct perceptron * p, const double * X);

