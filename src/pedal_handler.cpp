#include "pedal_handler.hpp"
void PedalHandler::set_sensor_ranges(float accel1limitlo, float accel1limithi, float accel2limitlo, float accel2limithi, float brakelimit1hi)
{
    accel1LIMITLO_ = calculateADCVolts(accel1limitlo);
    accel1LIMITHI_ = calculateADCVolts(accel1limithi);
    accel2LIMITLO_ = calculateADCVolts(accel2limitlo);
    accel2LIMITHI_ = calculateADCVolts(accel2limithi);
    brake1LIMITHI_ = calculateADCVolts(brakelimit1hi);
    a1Range=accel1LIMITHI_-accel1LIMITLO_; //help
    a2Range=accel2LIMITHI_-accel2LIMITLO_;
    Serial.printf("A1 lo hi: %f %f/nA2 lo hi: %f %f",accel1LIMITLO_,accel1LIMITHI_,accel2LIMITLO_,accel2LIMITHI_);
}

// initializes pedal's ADC
void PedalHandler::init_pedal_handler()
{
    pedal_ADC = ADC_SPI(DEFAULT_SPI_CS, DEFAULT_SPI_SPEED);
    //calculate sensor ranges
    set_sensor_ranges(START_ACCELERATOR_PEDAL_1,END_ACCELERATOR_PEDAL_1,START_ACCELERATOR_PEDAL_2,END_ACCELERATOR_PEDAL_2,BRAKE_ACTIVE);
}

int PedalHandler::calculate_torque(int16_t &motor_speed, int &max_torque)
{
    int calculated_torque = 0;
    int torque1 = map(round(accel1_), accel1LIMITLO_, accel1LIMITHI_, 0, max_torque);
    int torque2 = map(round(accel2_), accel2LIMITLO_, accel2LIMITHI_, 0, max_torque);

    // torque values are greater than the max possible value, set them to max
    if (torque1 > max_torque)
    {
        torque1 = max_torque;
    }
    if (torque2 > max_torque)
    {
        torque2 = max_torque;
    }
    // compare torques to check for accelerator implausibility
    calculated_torque = (torque1 + torque2) / 2;

    if (calculated_torque > max_torque)
    {
        calculated_torque = max_torque;
    }
    if (calculated_torque < 0)
    {
        calculated_torque = 0;
    }
    //#if DEBUG
    if (timer_debug_raw_torque->check())
    {
        // Serial.print("TORQUE REQUEST DELTA PERCENT: "); // Print the % difference between the 2 accelerator sensor requests
        // Serial.println(abs(torque1 - torque2) / (double) max_torque * 100);
        // Serial.print("MCU RAW TORQUE: ");
        // Serial.println(calculated_torque);
        // Serial.print("TORQUE 1: ");
        // Serial.println(torque1);
        // Serial.print("TORQUE 2: ");
        // Serial.println(torque2);
        // Serial.print("Accel 1: ");
        // Serial.println(accel1_);
        // Serial.print("Accel 2: ");
        // Serial.println(accel2_);
        // Serial.print("Brake1_ : ");
        // Serial.println(brake1_);
    }
    //#endif
    //keep this in our pocket for now V
    if (abs(motor_speed) <= 1000)
    {
        if (calculated_torque >= 600)
        {
            calculated_torque = 600; // ideally limit torque at low RPMs, see how high this number can be raised
        }
    }
    uint32_t calculated_power =  (calculated_torque/10)*motor_speed*0.104725;
    if(calculated_power>80000){
        calculated_torque=((80000*9.54)/motor_speed)*10;
    }
    return calculated_torque;
}

// returns true if the brake pedal is active
bool PedalHandler::read_pedal_values()
{
    /* Filter ADC readings */
    float raw_brake = pedal_ADC.read_adc(ADC_BRAKE_1_CHANNEL);
    float raw_accel1 = pedal_ADC.read_adc(ADC_ACCEL_1_CHANNEL);
    float raw_accel2 = pedal_ADC.read_adc(ADC_ACCEL_2_CHANNEL);

    // accel1_ = ALPHA * accel1_ + (1 - ALPHA) * raw_accel1;
    // accel2_ = ALPHA * accel2_ + (1 - ALPHA) * raw_accel2;
    // brake1_ = ALPHA * brake1_ + (1 - ALPHA) * raw_brake;
    accel1_ = calculateADCVolts(raw_accel1); //calculating sensor voltages
    accel2_ = calculateADCVolts(raw_accel2);
    brake1_ = calculateADCVolts(raw_brake);
    ratioA1=accel1_/a1Range;
    ratioA2=accel2_/a2Range;
    //ratioooo

#if DEBUG
    // Serial.println("reading pedal vals");
    // Serial.printf("val %f\n", brake1_);
    // Serial.printf("raw val %f\n", raw_brake);
    
    if (timer_debug_raw_torque->check()) 
    {
        Serial.print("ACCEL 1: ");
        Serial.println(accel1_);
        Serial.print("ACCEL 2: ");
        Serial.println(accel2_);
        Serial.print("BRAKE 1: ");
        Serial.println(brake1_);
        Serial.printf("TORQUE VALUES IF YOU WERE CALCULATING IT CURRENTLY: \n T1: %f\nT2: %f\n",(map(accel1_, accel1LIMITLO_, accel1LIMITHI_, 0, 2400)),(map(accel2_, accel2LIMITLO_, accel2LIMITHI_, 0, 2400)));
   }
#endif
    VCUPedalReadings.set_accelerator_pedal_1(uint16_t(accel1_*100));
    VCUPedalReadings.set_accelerator_pedal_2(uint16_t(accel2_*100));
    VCUPedalReadings.set_brake_transducer_1(uint16_t(brake1_*100));
    VCUPedalReadings.set_brake_transducer_2(uint16_t(brake1_*100));
    CAN_message_t tx_msg;

    // Send Main Control Unit pedal reading message
    VCUPedalReadings.write(tx_msg.buf);
    tx_msg.id = ID_VCU_PEDAL_READINGS;
    tx_msg.len = sizeof(VCUPedalReadings);

    // write out the actual accel command over CAN
    if (pedal_out->check())
    {
        WriteToDaqCAN(tx_msg);
    }
    // only uses front brake pedal
    brake_is_active_ = (brake1_ >= BRAKE_ACTIVE);
    return brake_is_active_;
}

void PedalHandler::verify_pedals(bool &accel_is_plausible, bool &brake_is_plausible, bool &accel_and_brake_plausible, bool &impl_occ)
{

    if (accel1_ < SENSOR_LO || accel1_ > SENSOR_HI)
    {
        accel_is_plausible = false;
#if DEBUG
        // Serial.println("T.4.2.10 1");
        // Serial.println(accel1_);
#endif
    }
    else if (accel2_ < SENSOR_LO || accel2_ > SENSOR_HI)
    {
        accel_is_plausible = false;
#if DEBUG
        // Serial.println("T.4.2.10 2");
        // Serial.println(accel2_);
#endif
    }
    // check that the pedals are reading within 10% of each other
    // sum of the two readings should be within 10% of the average travel
    // T.4.2.4
    else if ((accel1_ - (5.0 - accel2_)) >
             (accel1LIMITHI_ - accel1LIMITLO_ + accel2LIMITLO_ - accel1LIMITHI_) / 20)
    {
#if DEBUG
        // Serial.println("T.4.2.4");
        // Serial.printf("computed - %f\n", accel1_ - (4096 - accel2_));
        // Serial.printf("standard - %d\n", (END_ACCELERATOR_PEDAL_1 - START_ACCELERATOR_PEDAL_1 + START_ACCELERATOR_PEDAL_2 - END_ACCELERATOR_PEDAL_2) / 20);
#endif
        accel_is_plausible = false;
    }
    else
    {
#if DEBUG
        // Serial.println("T.4.2.4");
        // Serial.printf("computed - %f\n", accel1_ - (4096 - accel2_));
        // Serial.printf("standard - %d\n", (END_ACCELERATOR_PEDAL_1 - START_ACCELERATOR_PEDAL_1 + START_ACCELERATOR_PEDAL_2 - END_ACCELERATOR_PEDAL_2) / 20);
#endif
        // mcu_status.set_no_accel_implausability(true);
        accel_is_plausible = true;
    }

    // BSE check
    // EV.5.6
    // FSAE T.4.3.4
    if (brake1_ < 200 || brake1_ > 4000)
    {
        brake_is_plausible = false;
    }
    else
    {
        brake_is_plausible = true;
    }

    // FSAE EV.5.7
    // APPS/Brake Pedal Plausability Check
    if (
        (
            (accel1_ > ((END_ACCELERATOR_PEDAL_1 - START_ACCELERATOR_PEDAL_1) / 4 + START_ACCELERATOR_PEDAL_1)) ||
            (accel2_ > ((END_ACCELERATOR_PEDAL_2 - START_ACCELERATOR_PEDAL_2) / 4 + START_ACCELERATOR_PEDAL_2))) &&
        brake_is_active_)
    {
        Serial.println("dawg, dont press em both");
        accel_and_brake_plausible = false;
    }
    else if (
        (accel1_ < ((END_ACCELERATOR_PEDAL_1 - START_ACCELERATOR_PEDAL_1) / 20 + START_ACCELERATOR_PEDAL_1)) &&
        (accel2_ < ((END_ACCELERATOR_PEDAL_2 - START_ACCELERATOR_PEDAL_2) / 20 + START_ACCELERATOR_PEDAL_2)))
    {
        accel_and_brake_plausible = true;
        implausibility_occured_ = false; // only here do we want to reset this flag
    } else
    {
        accel_and_brake_plausible = true;
    }

    if((!accel_and_brake_plausible) || (!brake_is_plausible) || (!accel_is_plausible))
    {
        implausibility_occured_ = true;
    }

    impl_occ =implausibility_occured_;

}
float PedalHandler::calculateADCVolts(float adcReading){
    return ((adcReading/4096)*5);
}