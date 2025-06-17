<h1>Smart Traffic Management System (STMS)</h1>

<h2>Project Overview: A Vision for Smarter Streets</h2>
<p>
  As second-semester students in our <strong>Bachelor of Science in Artificial Intelligence (BSAI)</strong> program, our team of four was tasked with a challenging Object-Oriented Programming project. We chose to tackle a problem we see every day: inefficient traffic flow and the need for better enforcement on our roads. The <strong>Smart Traffic Management System (STMS)</strong> is the result of that ambition—a collaborative effort where each team member shouldered an equal share of the workload to bring a complex idea to life.
</p>
<p>
  This project is more than just code; it's our foundational attempt at using AI and robust software engineering to create a practical solution for a real-world challenge.
</p>

<h2>How It Works: The Brains Behind the System</h2>
<p>
  Our system is designed to replicate and enhance the functions of a modern traffic intersection, piece by piece:
</p>

<h3>The Eyes: Real-Time Computer Vision </h3>
<p>
  At its core, the system "sees" the road using live camera feeds. We integrated the powerful <strong>YOLOv8</strong> object detection model, which allows the application to identify, track, and count cars, buses, trucks, and motorcycles in real-time. This crucial first step transforms a chaotic visual scene into structured, usable data.
</p>

<h3>The Brain: Intelligent Decision-Making </h3>
<p>
  The data from the cameras feeds into the central application, which we built using <strong>C++ and the Qt framework</strong>. This is where the logic resides. Instead of relying on fixed, outdated timers, our system calculates the <strong>traffic density</strong> on each road. A congested lane with many cars gets a longer green light, while an empty lane gets less priority. This dynamic approach is the key to optimizing traffic flow and reducing unnecessary waiting.
</p>

<h3>The Action: Automated Control and Enforcement </h3>
<p>
  Based on its decisions, the system performs two key actions:
</p>
<ul>
  <li>
    <strong>Signal Control:</strong> It sends commands to change the traffic lights. For our prototype, this is visualized in the UI and can be connected to a physical set of LEDs via an <strong>Arduino</strong>, demonstrating a complete hardware-software loop.
  </li>
  <li>
    <strong>Violation Detection:</strong> If a vehicle runs a red light, the system automatically flags it, records the timestamp, and saves a snapshot as evidence. This creates an automated and impartial <strong>"check-and-balance" system</strong>.
  </li>
</ul>

<h2>A Collaborative Success</h2>
<p>
  This project's success is a testament to our <strong>teamwork</strong>. The four of us divided the core components—from the backend system logic and the demanding computer vision integration to the intricate front-end UI design and hardware communication. By <strong>distributing the work equally</strong>, we were able to focus on quality and deliver a comprehensive and polished application that we are incredibly proud of as second-semester BSAI students.
</p>

<h2>Future Roadmap & Planned Enhancements</h2>
<p>
  This project serves as a strong foundation for a more advanced system. Our future work will focus on expanding its intelligence, safety features, and enforcement capabilities:
</p>

<h3>Expanded Detection Capabilities</h3>
<ul>
  <li>
    Integrate <strong>pedestrian detection</strong> to manage crosswalks and enhance safety.
  </li>
  <li>
    Broaden the vehicle detection model to recognize a wider array of automobiles, such as <strong>three-wheelers and specialized commercial trucks</strong>.
  </li>
</ul>

<h3>Advanced Traffic Control Logic</h3>
<ul>
  <li>
    Implement an <strong>emergency vehicle detection</strong> module (e.g., for ambulances, fire trucks) to automatically grant them right-of-way.
  </li>
  <li>
    Develop an <strong>anomaly detection system</strong> to identify unusual road events like accidents, street racing, or fights, and potentially alert authorities.
  </li>
  <li>
    Integrate <strong>environmental data</strong> to adjust signal timing based on adverse conditions like heavy rain, fog, or extreme heat.
  </li>
</ul>

<h3>Enhanced Violation & Security System</h3>
<ul>
  <li>
    Implement <strong>Automatic Number Plate Recognition (ANPR)</strong> to read and log the license plates of violating vehicles for more effective enforcement.
  </li>
  <li>
    Explore advanced <strong>violator identification techniques</strong> to provide more comprehensive data for authorities, such as identifying unique vehicle characteristics.
  </li>
</ul>
