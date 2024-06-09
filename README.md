# Tugas Besar II3240 Rekayasa Sistem dan Teknologi Informasi : Trackee

Trackee adalah aplikasi manajemen gudang berbasis web dan mobile (Warehouse Management System) yang dirancang untuk meningkatkan efisiensi operasional dan akurasi dalam pengelolaan inventaris di gudang. Fitur utama termasuk rekomendasi penempatan barang berdasarkan tanggal kadaluarsa dan analisis pergerakan barang, pelacakan barang, dan otomasi perhitungan stok menggunakan Internet of Things (IoT) untuk mengurangi faktor human error. Trackee dirancang untuk membantu perusahaan mengurangi biaya operasional, meningkatkan akurasi inventaris, dan memberikan layanan yang lebih baik kepada pelanggan melalui pengelolaan gudang yang lebih efisien dan transparan.

## Kelompok 12 Kelas 02

- [18221046 Vincent Winarta](https://github.com/VincentWinarta)
- [18221112 Imanuel Raditya](https://github.com/imanuelraditya)
- [18221116 Miralistya Cahya F](https://github.com/miralistyacahya)
- [18221156 Fredrick Runie Taslim](https://github.com/fredrick03)
- [18221168 Rania Sasi Kirana](https://github.com/raniakiranaa)

## Tech Stack
Web application:
1. Next.js
2. ReactJS

Mobile application:
1. React Native

Database:
1. Supabase

## Database
### product
- product_id
- name
- brand
- category 

### rack
- rack_id
- category 

### item
- item_id
- product_id
- rack_id
- exp_date

### rack
- id
- rack_id
- loc_x
- loc_y
- item_id

### users
- id
- email
- username 
