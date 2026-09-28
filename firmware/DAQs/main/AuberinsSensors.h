// How to calculate pressure from voltage
// V = aP+b

// P = Pressure
// V = Voltage
// a = slope
// b = zero pressure intersection

// a= (4.5-0.5)/(75-0)=0.0533
// b = V-aP= 0.5

// 5 BAR sensor: AUBER-P205a
// P = (V-0.5)/0.0533

// Function to calculate pressure from voltage for 
// Boost
int calculatePressure5PSI(float voltage);

// 7 BAR sensro: Auber P207
// oil, fuel
// P=(V-0.5)/0.04
int calculatePressure7PSI(float voltage);
