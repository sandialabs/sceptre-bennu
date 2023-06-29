from dataclasses import dataclass

@dataclass
class BusData:
    name: str

    voltage_mag_p1: float = 0.0
    voltage_ang_p1: float = 0.0
    voltage_mag_p2: float = 0.0
    voltage_ang_p2: float = 0.0
    voltage_mag_p3: float = 0.0
    voltage_ang_p3: float = 0.0

@dataclass
class GeneratorData:
    name:   str
    bus:    str
    active: bool = False
    phases: str  = ''

    rated_apparent_power: float = 0.0
    active_power_output:  float = 0.0

    voltage_mag_p1:    float = 0.0
    voltage_ang_p1:    float = 0.0
    current_mag_p1:    float = 0.0
    current_ang_p1:    float = 0.0
    real_power_p1:     float = 0.0
    reactive_power_p1: float = 0.0
    voltage_mag_p2:    float = 0.0
    voltage_ang_p2:    float = 0.0
    current_mag_p2:    float = 0.0
    current_ang_p2:    float = 0.0
    real_power_p2:     float = 0.0
    reactive_power_p2: float = 0.0
    voltage_mag_p3:    float = 0.0
    voltage_ang_p3:    float = 0.0
    current_mag_p3:    float = 0.0
    current_ang_p3:    float = 0.0
    real_power_p3:     float = 0.0
    reactive_power_p3: float = 0.0
    voltage_mag_pn:    float = 0.0
    voltage_ang_pn:    float = 0.0
    current_mag_pn:    float = 0.0
    current_ang_pn:    float = 0.0
    real_power_pn:     float = 0.0
    reactive_power_pn: float = 0.0

@dataclass
class LoadData:
    name:   str
    bus:    str
    active: bool = False
    phases: str  = ''

    voltage_mag_p1:    float = 0.0
    voltage_ang_p1:    float = 0.0
    current_mag_p1:    float = 0.0
    current_ang_p1:    float = 0.0
    real_power_p1:     float = 0.0
    reactive_power_p1: float = 0.0
    voltage_mag_p2:    float = 0.0
    voltage_ang_p2:    float = 0.0
    current_mag_p2:    float = 0.0
    current_ang_p2:    float = 0.0
    real_power_p2:     float = 0.0
    reactive_power_p2: float = 0.0
    voltage_mag_p3:    float = 0.0
    voltage_ang_p3:    float = 0.0
    current_mag_p3:    float = 0.0
    current_ang_p3:    float = 0.0
    real_power_p3:     float = 0.0
    reactive_power_p3: float = 0.0
    voltage_mag_pn:    float = 0.0
    voltage_ang_pn:    float = 0.0
    current_mag_pn:    float = 0.0
    current_ang_pn:    float = 0.0
    real_power_pn:     float = 0.0
    reactive_power_pn: float = 0.0

@dataclass
class ShuntData:
    name:   str
    bus:    str
    active: bool = False
    phases: str  = ''

    rated_reactive_power: float = 0.0

    voltage_mag_p1:    float = 0.0
    voltage_ang_p1:    float = 0.0
    current_mag_p1:    float = 0.0
    current_ang_p1:    float = 0.0
    real_power_p1:     float = 0.0
    reactive_power_p1: float = 0.0
    voltage_mag_p2:    float = 0.0
    voltage_ang_p2:    float = 0.0
    current_mag_p2:    float = 0.0
    current_ang_p2:    float = 0.0
    real_power_p2:     float = 0.0
    reactive_power_p2: float = 0.0
    voltage_mag_p3:    float = 0.0
    voltage_ang_p3:    float = 0.0
    current_mag_p3:    float = 0.0
    current_ang_p3:    float = 0.0
    real_power_p3:     float = 0.0
    reactive_power_p3: float = 0.0
    voltage_mag_pn:    float = 0.0
    voltage_ang_pn:    float = 0.0
    current_mag_pn:    float = 0.0
    current_ang_pn:    float = 0.0
    real_power_pn:     float = 0.0
    reactive_power_pn: float = 0.0

@dataclass
class BranchData:
    name:       str
    source_bus: str
    dest_bus:   str
    active:     bool = False
    phases:     str  = ''

    resistance: float = 0.0
    reactance:  float = 0.0
    charging:   float = 0.0

    voltage_mag_src_p1:    float = 0.0
    voltage_ang_src_p1:    float = 0.0
    current_mag_src_p1:    float = 0.0
    current_ang_src_p1:    float = 0.0
    real_power_src_p1:     float = 0.0
    reactive_power_src_p1: float = 0.0
    voltage_mag_dst_p1:    float = 0.0
    voltage_ang_dst_p1:    float = 0.0
    current_mag_dst_p1:    float = 0.0
    current_ang_dst_p1:    float = 0.0
    real_power_dst_p1:     float = 0.0
    reactive_power_dst_p1: float = 0.0
    voltage_mag_src_p2:    float = 0.0
    voltage_ang_src_p2:    float = 0.0
    current_mag_src_p2:    float = 0.0
    current_ang_src_p2:    float = 0.0
    real_power_src_p2:     float = 0.0
    reactive_power_src_p2: float = 0.0
    voltage_mag_dst_p2:    float = 0.0
    voltage_ang_dst_p2:    float = 0.0
    current_mag_dst_p2:    float = 0.0
    current_ang_dst_p2:    float = 0.0
    real_power_dst_p2:     float = 0.0
    reactive_power_dst_p2: float = 0.0
    voltage_mag_src_p3:    float = 0.0
    voltage_ang_src_p3:    float = 0.0
    current_mag_src_p3:    float = 0.0
    current_ang_src_p3:    float = 0.0
    real_power_src_p3:     float = 0.0
    reactive_power_src_p3: float = 0.0
    voltage_mag_dst_p3:    float = 0.0
    voltage_ang_dst_p3:    float = 0.0
    current_mag_dst_p3:    float = 0.0
    current_ang_dst_p3:    float = 0.0
    real_power_dst_p3:     float = 0.0
    reactive_power_dst_p3: float = 0.0
    voltage_mag_src_pn:    float = 0.0
    voltage_ang_src_pn:    float = 0.0
    current_mag_src_pn:    float = 0.0
    current_ang_src_pn:    float = 0.0
    real_power_src_pn:     float = 0.0
    reactive_power_src_pn: float = 0.0
    voltage_mag_dst_pn:    float = 0.0
    voltage_ang_dst_pn:    float = 0.0
    current_mag_dst_pn:    float = 0.0
    current_ang_dst_pn:    float = 0.0
    real_power_dst_pn:     float = 0.0
    reactive_power_dst_pn: float = 0.0

@dataclass
class TransformerData:
    name:         str
    winding1_bus: str
    winding2_bus: str
    active:       bool = False
    phases:       str  = ''

    num_windings:         int = 0
    winding_pattern_wdg1: str = ''
    winding_pattern_wdg2: str = ''

    kva_rating_wdg1:     float = 0.0
    num_taps_wdg1:       int   = 0
    min_tap_wdg1:        float = 0.0
    max_tap_wdg1:        float = 0.0
    tap_setting_wdg1:    float = 0.0
    wdg1_resistance:     float = 0.0
    kva_rating_wdg2:     float = 0.0
    num_taps_wdg2:       int   = 0
    min_tap_wdg2:        float = 0.0
    max_tap_wdg2:        float = 0.0
    tap_setting_wdg2:    float = 0.0
    wdg2_resistance:     float = 0.0
    wdg3_resistance:     float = 0.0
    reactance_wdg1_wdg3: float = 0.0
    reactance_wdg2_wdg3: float = 0.0

    voltage_mag_wdg1_p1: float = 0.0
    voltage_ang_wdg1_p1: float = 0.0
    current_mag_wdg1_p1: float = 0.0
    current_ang_wdg1_p1: float = 0.0
    voltage_mag_wdg2_p1: float = 0.0
    voltage_ang_wdg2_p1: float = 0.0
    current_mag_wdg2_p1: float = 0.0
    current_ang_wdg2_p1: float = 0.0
    voltage_mag_wdg3_p1: float = 0.0
    voltage_ang_wdg3_p1: float = 0.0
    current_mag_wdg3_p1: float = 0.0
    current_ang_wdg3_p1: float = 0.0
    voltage_mag_wdg1_p2: float = 0.0
    voltage_ang_wdg1_p2: float = 0.0
    current_mag_wdg1_p2: float = 0.0
    current_ang_wdg1_p2: float = 0.0
    voltage_mag_wdg2_p2: float = 0.0
    voltage_ang_wdg2_p2: float = 0.0
    current_mag_wdg2_p2: float = 0.0
    current_ang_wdg2_p2: float = 0.0
    voltage_mag_wdg3_p2: float = 0.0
    voltage_ang_wdg3_p2: float = 0.0
    current_mag_wdg3_p2: float = 0.0
    current_ang_wdg3_p2: float = 0.0
    voltage_mag_wdg1_p3: float = 0.0
    voltage_ang_wdg1_p3: float = 0.0
    current_mag_wdg1_p3: float = 0.0
    current_ang_wdg1_p3: float = 0.0
    voltage_mag_wdg2_p3: float = 0.0
    voltage_ang_wdg2_p3: float = 0.0
    current_mag_wdg2_p3: float = 0.0
    current_ang_wdg2_p3: float = 0.0
    voltage_mag_wdg3_p3: float = 0.0
    voltage_ang_wdg3_p3: float = 0.0
    current_mag_wdg3_p3: float = 0.0
    current_ang_wdg3_p3: float = 0.0
    voltage_mag_wdg1_pn: float = 0.0
    voltage_ang_wdg1_pn: float = 0.0
    current_mag_wdg1_pn: float = 0.0
    current_ang_wdg1_pn: float = 0.0
    voltage_mag_wdg2_pn: float = 0.0
    voltage_ang_wdg2_pn: float = 0.0
    current_mag_wdg2_pn: float = 0.0
    current_ang_wdg2_pn: float = 0.0
    voltage_mag_wdg3_pn: float = 0.0
    voltage_ang_wdg3_pn: float = 0.0
    current_mag_wdg3_pn: float = 0.0
    current_ang_wdg3_pn: float = 0.0
    real_power_p1:       float = 0.0
    reactive_power_p1:   float = 0.0
    real_power_p2:       float = 0.0
    reactive_power_p2:   float = 0.0
    real_power_p3:       float = 0.0
    reactive_power_p3:   float = 0.0
    real_power_pn:       float = 0.0
    reactive_power_pn:   float = 0.0

@dataclass
class InverterData:
    name:   str
    bus:    str
    active: bool = False
    phases: str  = ''

    controller_name:     str  = ''
    volt_var_enable:     bool = False
    volt_var_curve_name: str  = ''
    volt_var_reference:  int  = 0

    rated_apparent_power: float = 0.0

    voltage_mag_p1:       float = 0.0
    voltage_ang_p1:       float = 0.0
    current_mag_p1:       float = 0.0
    current_ang_p1:       float = 0.0
    real_power_p1:        float = 0.0
    reactive_power_p1:    float = 0.0
    voltage_mag_p2:       float = 0.0
    voltage_ang_p2:       float = 0.0
    current_mag_p2:       float = 0.0
    current_ang_p2:       float = 0.0
    real_power_p2:        float = 0.0
    reactive_power_p2:    float = 0.0
    voltage_mag_p3:       float = 0.0
    voltage_ang_p3:       float = 0.0
    current_mag_p3:       float = 0.0
    current_ang_p3:       float = 0.0
    real_power_p3:        float = 0.0
    reactive_power_p3:    float = 0.0
    voltage_mag_pn:       float = 0.0
    voltage_ang_pn:       float = 0.0
    current_mag_pn:       float = 0.0
    current_ang_pn:       float = 0.0
    real_power_pn:        float = 0.0
    reactive_power_pn:    float = 0.0
    current_mag_total:    float = 0.0
    real_power_total:     float = 0.0
    reactive_power_total: float = 0.0
    curve_volt_pt_1:      float = 0.0
    curve_var_pt_1:       float = 0.0
    curve_volt_pt_2:      float = 0.0
    curve_var_pt_2:       float = 0.0
    curve_volt_pt_3:      float = 0.0
    curve_var_pt_3:       float = 0.0
    curve_volt_pt_4:      float = 0.0
    curve_var_pt_4:       float = 0.0
    curve_volt_pt_5:      float = 0.0
    curve_var_pt_5:       float = 0.0
    curve_volt_pt_6:      float = 0.0
    curve_var_pt_6:       float = 0.0
