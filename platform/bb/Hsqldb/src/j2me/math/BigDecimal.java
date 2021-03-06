package j2me.math;

public class BigDecimal {

    public final static int ROUND_HALF_DOWN =    5;
	
	public BigDecimal(double val) {
		//TODO: BigDecimal(double val)
	}
	
	public BigDecimal(long val) {
		//intVal = BigInteger.valueOf(val);
	}
	
    public BigDecimal(String val) {
        this(val.toCharArray(), 0, val.length());
    }

    public BigDecimal(char[] in, int offset, int len) {
    }
    public BigDecimal(BigInteger val) {
        //intVal = val;
    }
    public BigDecimal(BigInteger unscaledVal, int scale) {
        // Negative scales are now allowed
        //intVal = unscaledVal;
        //this.scale = scale;
    }
	
    public BigDecimal add(BigDecimal augend) {
//        BigDecimal arg[] = new BigDecimal[2];
//        arg[0] = this;  arg[1] = augend;
//        matchScale(arg);
//        return new BigDecimal(arg[0].intVal.add(arg[1].intVal), arg[0].scale);
    	
        return new BigDecimal(0);
    }

    public BigDecimal negate() {
//        BigDecimal result = new BigDecimal(intVal.negate(), scale);
//        result.precision = precision;
//        return result;
    	return new BigDecimal(0);
    }
    
    public BigDecimal multiply(BigDecimal multiplicand) {
//        BigDecimal result = new BigDecimal(intVal.multiply(multiplicand.intVal), 0);
//        result.scale = checkScale((long)scale+multiplicand.scale);
//        return result;
    	return new BigDecimal(0);
    }
    
    public int scale() {
//        return scale;
    	return 0;
    }
    
    public int signum() {
        //return intVal.signum();
    	return 0;
    }
    
    public BigDecimal divide(BigDecimal divisor, int scale, int roundingMode) {
    	return new BigDecimal(0);
    }

    public BigDecimal divide(BigDecimal divisor, int roundingMode) {
        //return this.divide(divisor, scale, roundingMode);
    	return new BigDecimal(0);
    }
    
    public BigDecimal subtract(BigDecimal subtrahend) {
        //BigDecimal arg[] = new BigDecimal[2];
        //arg[0] = this;  arg[1] = subtrahend;
        //matchScale(arg);
        //return new BigDecimal(arg[0].intVal.subtract(arg[1].intVal),
        //                      arg[0].scale);
    	return new BigDecimal(0);
    }
    
    public int compareTo(BigDecimal val) {
    	return 0;
    }
    
    public static BigDecimal valueOf(long val) {
    	return valueOf(val, 0);
    }

    public static BigDecimal valueOf(long unscaledVal, int scale) {
        //if (scale == 0) {
        //   if (unscaledVal == 0)
	    //   return ZERO;
        //   if (unscaledVal == 1)
        //       return ONE;
        //   if (unscaledVal == 10)
        //       return TEN;
        //}
        //return new BigDecimal(BigInteger.valueOf(unscaledVal), scale);
    	
    	return new BigDecimal(0);
    }

    public BigDecimal setScale(int newScale, int roundingMode) {
    	return new BigDecimal(0);
    }
    
    public BigInteger toBigInteger() {
        // force to an integer, quietly
        //return this.setScale(0, ROUND_DOWN).intVal;
    	return new BigInteger("0");
    }
    
    public double doubleValue(){
    	/* Somewhat inefficient, but guaranteed to work. */
    	return Double.valueOf(this.toString()).doubleValue();
    }

    public BigInteger unscaledValue() {
//        return intVal;
    	return new BigInteger("0"); 
    }
    
   
}
