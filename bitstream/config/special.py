from bitstream.config.base import BaseConfigModule

class SpecialArrayConfig(BaseConfigModule):
    """Special array (PE) configuration module."""

    FIELD_MAP = [
        # data_type: "fp16" -> 0, "fp32" -> 1
        ("data_type", 2, lambda x: 0 if x == "fp16" else 1),

        # index_end: 3-bit integer
        ("index_end", 3),

        # inport enable signals (1 bit each)
        ("inport0_enbale", 1),
        ("inport1_enbale", 1),
        ("inport2_enbale", 1),

        # inport source IDs (lists -> binary vectors)
        ("inport0_src_id", 4),  # width = number of supported IDs (example: 4)
        ("inport1_src_id", 4),
        ("inport2_src_id", 4),

        # output port enable
        ("outport_enbale", 1),

        # outport_mode: "col" -> 0, "row" -> 1
        ("outport_mode", 1, lambda x: 0 if x == "col" else 1),

        # outport_fp32to16: "true" -> 1, "false" -> 0
        ("outport_fp32to16", 1, lambda x: 1 if str(x).lower() == "true" else 0),
    ]

    def from_json(self, cfg: dict):
        """Load configuration values from JSON."""
        special = cfg.get("special_array", {})
        super().from_json(special)