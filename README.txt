XOC DIA OOP SIMULATOR
=====================

1. Mục đích
-----------
Project này mô phỏng cược chẵn/lẻ trong xóc đĩa bằng C++17 theo hướng OOP.
Ba chiến thuật được hỗ trợ:
- Flatbet
- Martingale
- Oscar's Grind

Chương trình có 2 chế độ:
- TUI tương tác trong terminal
- CLI để chạy mô phỏng tái lập bằng tham số dòng lệnh


2. Cấu trúc thư mục
-------------------
- CMakeLists.txt                Cấu hình build
- include/                      Header files
- src/                          Mã nguồn chính
- tests/strategy_tests.cpp      Test đơn giản cho strategy
- report/xocdia_3_strategies.tex  Báo cáo LaTeX
- report/xocdia_3_strategies.pdf  Báo cáo PDF


3. Yêu cầu môi trường
---------------------
Cần có:
- g++ hỗ trợ C++17
- cmake >= 3.16
- pkg-config
- ncursesw
- lualatex

Trong workspace này, mọi lệnh nên chạy kèm:
source ~/.bashrc


4. Build
--------
Build project:

source ~/.bashrc; cmake -S . -B build
source ~/.bashrc; cmake --build build

Sau khi build xong sẽ có:
- build/xocdia
- build/strategy_tests


5. Chạy TUI
-----------
Mở giao diện tương tác:

source ~/.bashrc; ./build/xocdia

Ý nghĩa các trường chính:
- Players: số người chơi mô phỏng độc lập
- Rounds: số lượt tối đa cho mỗi người chơi
- Bankroll: vốn ban đầu
- Base bet: mức cược cơ bản
- Payout: hệ số lời ròng khi thắng
- Probability red: xác suất một quân vị ra đỏ
- Side: cửa đặt cược, gồm even / odd / random
- Seed: seed ngẫu nhiên để tái lập kết quả
- Table limit: giới hạn cược, 0 nghĩa là không giới hạn explicit

Lưu ý:
- Tổng quy mô mô phỏng gần đúng bằng Players x Rounds
- Với payout = 0.96 và game công bằng p = 0.5, kỳ vọng toán học là âm


6. Phím điều khiển trong TUI
----------------------------
Màn hình chính:
- Up / Down: chọn field
- Left / Right: tăng giảm nhanh giá trị
- Enter: nhập giá trị trực tiếp
- R: chạy mô phỏng
- D: mở demo từng bước
- S: lưu CSV
- C: xóa lịch sử kết quả
- L: đổi ngôn ngữ EN / VI
- ?: mở help
- Ctrl + D: thoát

Demo từng bước:
- Space: chạy 1 lượt
- P: tự động chạy 10 lượt
- N: reset demo
- C: xóa lịch sử lượt
- X: đóng demo

Fun keys:
- F: chuyển nhanh sang Flatbet
- M: chuyển nhanh sang Martingale
- O: chuyển nhanh sang Oscar's Grind
- ! : random seed
- Có easter egg bằng mã classic arcade trong help

Yêu cầu terminal:
- Tối thiểu 88x24


7. Chạy CLI
-----------
Ví dụ chạy một chiến thuật:

source ~/.bashrc; ./build/xocdia --strategy flat --players 1000 --rounds 200 --bankroll 1000 --base 10 --payout 0.96 --seed 42

Ví dụ chạy cả 3 chiến thuật:

source ~/.bashrc; ./build/xocdia --strategy all --players 5000 --rounds 200 --bankroll 1000 --base 10 --payout 0.96 --seed 42

Ví dụ lưu CSV:

source ~/.bashrc; ./build/xocdia --strategy all --players 5000 --rounds 200 --bankroll 1000 --base 10 --payout 0.96 --seed 42 --csv results/summary.csv

Các tham số hỗ trợ:
- --strategy flat|martingale|oscars|all
- --players N
- --sessions N            alias cũ của --players
- --rounds N
- --bankroll VALUE
- --base VALUE
- --payout VALUE
- --prob-red VALUE
- --side even|odd|random
- --seed N
- --limit VALUE
- --csv PATH


8. Giải thích ba strategy
-------------------------
Flatbet
- Luôn cược đúng base bet ở mọi lượt
- Dễ hiểu nhất, ổn định nhất, ít biến động nhất
- Không có cơ chế gỡ lỗ

Martingale
- Thua thì nhân đôi mức cược
- Thắng thì quay về base bet
- Có thể tạo cảm giác hồi vốn nhanh
- Rủi ro lớn vì chuỗi thua làm mức cược tăng rất mạnh

Oscar's Grind
- Sau thua: giữ nguyên mức cược hiện tại
- Sau thắng: tăng thêm một base unit nếu chưa đạt mục tiêu chu kỳ
- Khi lợi nhuận chu kỳ đạt ít nhất một base unit thì reset
- Tăng chậm hơn Martingale nhưng vẫn không đổi được kỳ vọng âm của game


9. Kết quả và CSV
-----------------
Trong TUI:
- Mỗi lần bấm R, kết quả mới được append vào bảng lịch sử
- Có thể bấm C để xóa toàn bộ lịch sử kết quả
- Khi bấm S trong TUI, CSV sẽ được lưu vào thư mục results/ với tên mặc định tự sinh

Trong animation chạy nhanh:
- Có preview W/L của lượt hiện tại
- Có W count / L count
- Có chuỗi Last 5
- Có lời giải thích ngắn theo chiến thuật đang preview


10. Test
--------
Chạy test:

source ~/.bashrc; ./build/strategy_tests

Hoặc:

source ~/.bashrc; ctest --test-dir build --output-on-failure

Các test hiện có kiểm tra:
- Flatbet luôn trả về base bet
- Martingale gấp đôi sau thua và reset sau thắng
- Oscar's Grind tăng sau thắng và reset đúng chu kỳ


11. Báo cáo LaTeX
-----------------
File nguồn:
- report/xocdia_3_strategies.tex

Build PDF:

source ~/.bashrc; lualatex -interaction=nonstopmode -output-directory report report/xocdia_3_strategies.tex
source ~/.bashrc; lualatex -interaction=nonstopmode -output-directory report report/xocdia_3_strategies.tex

Ghi chú:
- Bản báo cáo hiện dùng Unicode tiếng Việt có dấu
- Nếu build lần đầu chậm hơn bình thường, đó có thể là lúc LuaLaTeX tạo font database


12. Một vài nhận xét thực tế
----------------------------
- Strategy không đổi xác suất thắng của game, chỉ đổi cách phân phối lời lỗ
- Payout nhỏ hơn 1 làm EV âm trên mỗi đơn vị tiền cược
- Martingale và Oscar's Grind có thể nhìn đẹp ở một số phiên ngắn, nhưng drawdown và risk of ruin thường nặng hơn Flatbet
- Nếu mục tiêu là ổn định và dễ kiểm soát, Flatbet là baseline tốt nhất để so sánh
