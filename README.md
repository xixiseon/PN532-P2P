# PN532 P2P Demo

This STM32F10x demo provides separate Keil targets for a PN532 NFC-DEP
Initiator and Target. Both targets communicate with the PN532 through HSU
(UART). The Initiator uses Passive 106 kbps by default and can fall back to
Active 106 kbps according to the selected test policy.

## Keil targets

- `Initiator`: builds `P2PInitiator` with `P2P_INITIATOR_MODE`.
- `Target`: builds `P2PTarget` with `P2P_TARGET_MODE`.
- Compiler: ARMCC 5.06 update 7.

## IO connections

Use the same PN532 HSU wiring for both the Initiator and Target boards.

| STM32F10x | Direction | PN532 module | Purpose |
| --- | --- | --- | --- |
| `PA2 / USART2_TX` | STM32 to PN532 | `RX` / `HSU_RX` | Host command data |
| `PA3 / USART2_RX` | PN532 to STM32 | `TX` / `HSU_TX` | PN532 response data |
| `GND` | Common | `GND` | Common signal reference |
| Module supply | Power | `VCC` | Use the voltage specified by the PN532 module |
| `PC2` | STM32 output | Optional user LED | Activity indication, active low |

UART settings are `115200`, 8 data bits, no parity, 1 stop bit, and no hardware
flow control. TX and RX must be crossed as shown above. Select HSU/UART mode on
the PN532 module before power-up; switch names and levels vary by module, so use
the module's own mode-selection table. This demo does not connect PN532 `IRQ`
or `RST/RESET`; reception uses the USART2 RX and IDLE interrupts.

Keep the PN532 UART logic levels compatible with the STM32's 3.3 V IO and always
connect the grounds. Do not infer the module supply voltage from its UART logic
level; follow the module hardware specification.

## Link test policy

The Initiator defaults to `P2P_LINK_POLICY_AUTO`. Override `P2P_LINK_POLICY` in
the Keil C/C++ preprocessor definitions for controlled tests.

| Policy | Behavior after failure |
| --- | --- |
| `P2P_LINK_POLICY_AUTO` | Start Passive; try Active after a Passive activation or exchange failure; return to Passive after an Active failure. |
| `P2P_LINK_POLICY_FORCE_PASSIVE` | Always use Passive and run the same recovery path as AUTO. |
| `P2P_LINK_POLICY_FORCE_ACTIVE` | Always use Active and run the same recovery path as AUTO. |
| `P2P_LINK_POLICY_PASSIVE_RETRY_RECOVER` | Always retry Passive after the same Abort and DEP session rebuild used by AUTO. |

Run at least 200 complete sessions per policy with the same boards, antenna
position, distance, power, payload, 106 kbps rate, and movement path. Repeat
the comparison for fixed-position and moving tests. The demo prints the session
ID, mode, result, raw PN532 status, recovery action, switch reason, and a summary
every 100 activation attempts.

Active should be considered more reliable only when `FORCE_ACTIVE` is clearly
better than `FORCE_PASSIVE` under the same conditions, AUTO repeatedly succeeds
after switching to Active, and `PASSIVE_RETRY_RECOVER` remains clearly worse
than the Active result. If Passive recovery approaches Active, the main benefit
comes from aborting and rebuilding the DEP session. If only moving tests improve,
the result is specific to dynamic antenna position rather than static range.
